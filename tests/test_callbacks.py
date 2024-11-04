import test_callbacks_ext as t
import gc


def test_callbacks():
    pub1 = t.publisher()
    pub2 = t.publisher()
    record = []

    def sub1(x):
        record.append(x + 10)

    def sub2(x):
        record.append(x + 20)

    pub1.subscribe(sub1)
    pub2.subscribe(sub2)
    for pub in (pub1, pub2):
        pub.subscribe(record.append)

    pub1.emit(1)
    assert record == [11, 1]
    del record[:]

    pub2.emit(2)
    assert record == [22, 2]
    del record[:]

    pub1_w, pub2_w = t.registry.keys()  # weakrefs to pub1, pub2
    assert pub1_w() is pub1
    assert pub2_w() is pub2
    assert t.registry[pub1_w].subscribers == {sub1, record.append}
    assert t.registry[pub2_w].subscribers == {sub2, record.append}

    # NB: this `record.append` is a different object than the one we subscribed
    # above, so we're testing the normalization logic in unsubscribe_policy
    pub1.unsubscribe(record.append)
    assert t.registry[pub1_w].subscribers == {sub1}
    pub1.emit(3)
    assert record == [13]
    del record[:]

    del pub, pub1
    gc.collect()
    gc.collect()
    assert pub1_w() is None
    assert pub2_w() is pub2
    assert t.registry.keys() == {pub2_w}

    pub2.emit(4)
    assert record == [24, 4]
    del record[:]

    del pub2
    gc.collect()
    gc.collect()
    assert pub2_w() is None
    assert not t.registry
