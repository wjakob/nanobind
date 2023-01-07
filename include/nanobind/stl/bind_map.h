/*
    nanobind/stl/bind_map.h: Automatic creation of bindings for map-style containers

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename Map>
struct KeysView
{
    explicit KeysView(Map &map) : map(map) {}
    size_t len() { return map.size(); }
    iterator iter() { return make_key_iterator(type<Map>(), "make_key_iterator", map.begin(), map.end()); }
    bool contains(const typename Map::key_type &k) { return map.find(k) != map.end(); }
    bool contains(const object &) { return false; }
    Map &map;
};

template <typename Map>
struct ValuesView
{
    explicit ValuesView(Map &map) : map(map) {}
    size_t len() { return map.size(); }
    iterator iter() { return make_value_iterator(type<Map>(), "make_value_iterator", map.begin(), map.end()); }
    Map &map;
};

template <typename Map>
struct ItemsView
{
    explicit ItemsView(Map &map) : map(map) {}
    size_t len() { return map.size(); }
    iterator iter() { return make_iterator(type<Map>(), "make_item_iterator", map.begin(), map.end()); }
    Map &map;
};

NAMESPACE_END(detail)

template <typename Map, typename... Args>
class_<Map> bind_map(handle scope, const char *name, Args &&...args)
{
    using KeyType = typename Map::key_type;
    using MappedType = typename Map::mapped_type;
    using KeysView = detail::KeysView<Map>;
    using ValuesView = detail::ValuesView<Map>;
    using ItemsView = detail::ItemsView<Map>;
    using Class_ = class_<Map>;

    Class_ cl(scope, name, std::forward<Args>(args)...);

    // install KeysView directly into the map class scope
    class_<KeysView> keys_view(cl, "KeysView");
    keys_view.def("__len__", &KeysView::len);
    keys_view.def("__iter__", 
                  &KeysView::iter,
                  keep_alive<0, 1>()
    );
    keys_view.def("__contains__",
                  static_cast<bool (KeysView::*)(const KeyType &)>(&KeysView::contains));
    // Fallback for when the object is not of the key type
    keys_view.def("__contains__",
                  static_cast<bool (KeysView::*)(const object &)>(&KeysView::contains));

    // same in-place installation with ValuesView...
    class_<ValuesView> values_view(cl, "ValuesView");
    values_view.def("__len__", &ValuesView::len);
    values_view.def("__iter__",
                    &ValuesView::iter,
                    keep_alive<0, 1>()
    );

    // and with ItemsView, too.
    class_<ItemsView> items_view(scope, "ItemsView");
    items_view.def("__len__", &ItemsView::len);
    items_view.def("__iter__",
                   &ItemsView::iter,
                   keep_alive<0, 1>()
    );

    cl.def(init<>());

    cl.def(
        "__bool__", 
        [](const Map &m) -> bool { return !m.empty(); },
        "Check whether the map is nonempty");

    cl.def(
        "__iter__",
        [](Map &m) { return make_key_iterator(type<Map>(), "make_key_iterator", m.begin(), m.end()); },
        keep_alive<0, 1>() /* Essential: keep map alive while iterator exists */
    );

    cl.def(
        "keys",
        [](Map &m){ return new detail::KeysView<Map>(m); },
        keep_alive<0, 1>()
    );

    cl.def(
        "values",
        [](Map &m) { return new detail::ValuesView<Map>(m); },
        keep_alive<0, 1>()
    );

    cl.def(
        "items",
        [](Map &m) { return new detail::ItemsView<Map>(m); },
        keep_alive<0, 1>()
    );

    cl.def(
        "__getitem__",
        [](Map &m, const KeyType &k) -> MappedType & {
            auto it = m.find(k);
            if (it == m.end()) {
                throw key_error();
            }
            return it->second;
        },
        rv_policy::reference_internal // ref + keepalive
    );

    cl.def("__contains__", [](const Map &m, const KeyType &k) -> bool {
        auto it = m.find(k);
        if (it == m.end()) {
            return false;
        }
        return true; 
    });
    // Fallback for when the object is not of the key type
    cl.def("__contains__", [](const Map &, handle) -> bool { return false; });

    // Assignment provided only if the type is copyable
    if constexpr(std::is_copy_assignable_v<MappedType> ||
                 std::is_copy_constructible_v<MappedType>){
        // Map assignment when copy-assignable: just copy the value
        if constexpr(std::is_copy_assignable_v<MappedType>){
            cl.def("__setitem__", [](Map &m, const KeyType &k, const MappedType &v)
            {
                auto it = m.find(k);
                    if (it != m.end()) {
                        it->second = v;
                    } else {
                    m.emplace(k, v);
                }
            });
        }
        // Not copy-assignable, but still copy-constructible: we can update the value by erasing and
        // reinserting
        else{
            cl.def("__setitem__", [](Map &m, const KeyType &k, const MappedType &v)
            {
                // We can't use m[k] = v; because value type might not be default
                // constructible
                auto r = m.emplace(k, v);
                    if (!r.second) {
                        // value type is not copy assignable so the only way to insert it is to
                        // erase it first.
                        m.erase(r.first);
                        m.emplace(k, v);
                    } 
                }
            );
        }
    }

    cl.def("__delitem__", [](Map &m, const KeyType &k) {
        auto it = m.find(k);
        if (it == m.end()) {
            throw key_error();
        }
        m.erase(it); 
    });

    cl.def("__len__", &Map::size);

    return cl;
}

NAMESPACE_END(NB_NAMESPACE)
