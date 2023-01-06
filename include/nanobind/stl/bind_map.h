/*
    nanobind/stl/bind_map.h: nb::bind_map()

    This implementation is a port from pybind11 with minimal adjustments.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>
#include <nanobind/stl/unique_ptr.h> // TODO: Opt out of unique_ptr (used for returning key/value views)?
#include <nanobind/stl/string.h>     // TODO: Opt out of string (used for constructing "(Keys|Values|Items)View[type]"-style names to bind to the module scope)

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// only available since C++20
using std::remove_cv_t;
using std::remove_reference_t;

template <class T>
struct remove_cvref
{
    using type = remove_cv_t<remove_reference_t<T>>;
};
template <class T>
using remove_cvref_t = typename remove_cvref<T>::type;

template <typename, typename, typename... Args>
void map_if_insertion_operator(const Args &...) {}
template <typename, typename, typename... Args>
void map_assignment(const Args &...) {}

// Map assignment when copy-assignable: just copy the value
template <typename Map, typename Class_>
void map_assignment(
    std::enable_if_t<std::is_copy_assignable<typename Map::mapped_type>::value, Class_> &cl)
{
    using KeyType = typename Map::key_type;
    using MappedType = typename Map::mapped_type;

    cl.def("__setitem__", [](Map &m, const KeyType &k, const MappedType &v)
           {
    auto it = m.find(k);
    if (it != m.end()) {
      it->second = v;
    } else {
      m.emplace(k, v);
    } });
}

// Not copy-assignable, but still copy-constructible: we can update the value by
// erasing and reinserting
template <typename Map, typename Class_>
void map_assignment(
    std::enable_if_t<!std::is_copy_assignable<typename Map::mapped_type>::value &&
                         std::is_copy_constructible<typename Map::mapped_type>::value,
                     Class_> &cl)
{
    using KeyType = typename Map::key_type;
    using MappedType = typename Map::mapped_type;

    cl.def("__setitem__", [](Map &m, const KeyType &k, const MappedType &v)
           {
    // We can't use m[k] = v; because value type might not be default
    // constructable
    auto r = m.emplace(k, v);
    if (!r.second) {
      // value type is not copy assignable so the only way to insert it is to
      // erase it first...
      m.erase(r.first);
      m.emplace(k, v);
    } });
}

// template KeysView, ValuesView and ItemsView on their key type, so as
// to bind different keys views for different key types
template <typename KeyType>
struct keys_view
{
    virtual size_t len() = 0;
    virtual iterator iter() = 0;
    virtual bool contains(const KeyType &k) = 0;
    virtual bool contains(const object &k) = 0;
    virtual ~keys_view() = default;
};

template <typename MappedType>
struct values_view
{
    virtual size_t len() = 0;
    virtual iterator iter() = 0;
    virtual ~values_view() = default;
};

template <typename KeyType, typename MappedType>
struct items_view
{
    virtual size_t len() = 0;
    virtual iterator iter() = 0;
    virtual ~items_view() = default;
};

template <typename Map, typename KeysView>
struct KeysViewImpl : public KeysView
{
    explicit KeysViewImpl(Map &map) : map(map) {}
    size_t len() override { return map.size(); }
    // TODO: Does type<Map> at any point return nullptr (since Map is not bound yet)?
    iterator iter() override { return make_key_iterator(type<Map>(), "make_key_iterator", map.begin(), map.end()); }
    bool contains(const typename Map::key_type &k) override
    {
        return map.find(k) != map.end();
    }
    bool contains(const object &) override { return false; }
    Map &map;
};

template <typename Map, typename ValuesView>
struct ValuesViewImpl : public ValuesView
{
    explicit ValuesViewImpl(Map &map) : map(map) {}
    size_t len() override { return map.size(); }
    // TODO: Does type<Map> at any point return nullptr in this implementation (since Map is not bound yet)?
    iterator iter() override
    {
        return make_value_iterator(type<Map>(), "make_value_iterator", map.begin(), map.end());
    }
    Map &map;
};

template <typename Map, typename ItemsView>
struct ItemsViewImpl : public ItemsView
{
    explicit ItemsViewImpl(Map &map) : map(map) {}
    size_t len() override { return map.size(); }
    // TODO: Does type<Map> at any point return nullptr (since Map is not bound yet)?
    iterator iter() override { return make_iterator(type<Map>(), "make_item_iterator", map.begin(), map.end()); }
    Map &map;
};

NAMESPACE_END(detail)

template <typename Map, typename... Args>
class_<Map> bind_map(handle scope, const char *name, Args &&...args)
{
    using KeyType = typename Map::key_type;
    using MappedType = typename Map::mapped_type;
    // we do not want a different key type / mapped type between
    // e.g. double and const double, or double and double& -> strip cv qualifier
    using StrippedKeyType = detail::remove_cvref_t<KeyType>;
    using StrippedMappedType = detail::remove_cvref_t<MappedType>;
    using KeysView = detail::keys_view<StrippedKeyType>;
    using ValuesView = detail::values_view<StrippedMappedType>;
    using ItemsView = detail::items_view<StrippedKeyType, StrippedMappedType>;
    using Class_ = class_<Map>;

    Class_ cl(scope, name, std::forward<Args>(args)...);
    static constexpr auto key_type_descr = detail::make_caster<KeyType>::Name;
    static constexpr auto mapped_type_descr = detail::make_caster<MappedType>::Name;
    std::string key_type_str(key_type_descr.text), mapped_type_str(mapped_type_descr.text);

    // If key type isn't properly wrapped, fall back to C++ names
    if (key_type_str == "%")
    {
        key_type_str = std::string(typeid(KeyType).name());
    }
    // Similarly for value type:
    if (mapped_type_str == "%")
    {
        mapped_type_str = std::string(typeid(MappedType).name());
    }

    // Wrap KeysView[KeyType] if it wasn't already wrapped
    if (!detail::nb_type_lookup(&typeid(KeysView)))
    {
        class_<KeysView> keys_view(scope, ("KeysView[" + key_type_str + "]").c_str());
        keys_view.def("__len__", &KeysView::len);
        keys_view.def("__iter__", &KeysView::iter,
                      keep_alive<0, 1>() /* Essential: keep view alive while
                                            iterator exists */
        );
        keys_view.def(
            "__contains__",
            static_cast<bool (KeysView::*)(const KeyType &)>(&KeysView::contains));
        // Fallback for when the object is not of the key type
        keys_view.def(
            "__contains__",
            static_cast<bool (KeysView::*)(const object &)>(&KeysView::contains));
    }
    // Similarly for ValuesView:
    if (!detail::nb_type_lookup(&typeid(ValuesView)))
    {
        class_<ValuesView> values_view(scope, ("ValuesView[" + mapped_type_str + "]").c_str());
        values_view.def("__len__", &ValuesView::len);
        values_view.def("__iter__", &ValuesView::iter,
                        keep_alive<0, 1>() /* Essential: keep view alive while
                                              iterator exists */
        );
    }
    // Similarly for ItemsView:
    if (!detail::nb_type_lookup(&typeid(ItemsView)))
    {
        class_<ItemsView> items_view(
            scope,
            ("ItemsView[" + key_type_str + ", ").append(mapped_type_str + "]").c_str());
        items_view.def("__len__", &ItemsView::len);
        items_view.def("__iter__", &ItemsView::iter,
                       keep_alive<0, 1>() /* Essential: keep view alive while
                                             iterator exists */
        );
    }

    cl.def(init<>());

    cl.def(
        "__bool__", [](const Map &m) -> bool
        { return !m.empty(); },
        "Check whether the map is nonempty");

    cl.def(
        "__iter__", [](Map &m)
        { return make_key_iterator(type<Map>(), "make_key_iterator", m.begin(), m.end()); },
        keep_alive<0, 1>() /* Essential: keep map alive while iterator exists */
    );

    cl.def(
        "keys",
        [](Map &m)
        {
            return std::unique_ptr<KeysView>(
                new detail::KeysViewImpl<Map, KeysView>(m));
        },
        keep_alive<0, 1>() /* Essential: keep map alive while view exists */
    );

    cl.def(
        "values",
        [](Map &m)
        {
            return std::unique_ptr<ValuesView>(
                new detail::ValuesViewImpl<Map, ValuesView>(m));
        },
        keep_alive<0, 1>() /* Essential: keep map alive while view exists */
    );

    cl.def(
        "items",
        [](Map &m)
        {
            return std::unique_ptr<ItemsView>(
                new detail::ItemsViewImpl<Map, ItemsView>(m));
        },
        keep_alive<0, 1>() /* Essential: keep map alive while view exists */
    );

    cl.def(
        "__getitem__",
        [](Map &m, const KeyType &k) -> MappedType &
        {
            auto it = m.find(k);
            if (it == m.end())
            {
                throw key_error();
            }
            return it->second;
        },
        rv_policy::reference_internal // ref + keepalive
    );

    cl.def("__contains__", [](Map &m, const KeyType &k) -> bool
           {
    auto it = m.find(k);
    if (it == m.end()) {
      return false;
    }
    return true; });
    // Fallback for when the object is not of the key type
    cl.def("__contains__", [](Map &, const object &) -> bool
           { return false; });

    // Assignment provided only if the type is copyable
    detail::map_assignment<Map, Class_>(cl);

    cl.def("__delitem__", [](Map &m, const KeyType &k)
           {
    auto it = m.find(k);
    if (it == m.end()) {
      throw key_error();
    }
    m.erase(it); });

    cl.def("__len__", &Map::size);

    return cl;
}

NAMESPACE_END(NB_NAMESPACE)
