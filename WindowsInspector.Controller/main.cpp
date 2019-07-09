#include <iostream>
#include <Windows.h>
#include "list.h"

struct DataItem {
    LIST_ENTRY Entry;
    int Value;

    DataItem(int val) : Value(val) {
        Entry.Blink = NULL;
        Entry.Flink = NULL;
    }
};

DataItem& ToDataItem(PLIST_ENTRY entry) {
    DataItem* item = CONTAINING_RECORD(entry, DataItem, Entry);
    return *item;
}

struct DataItemList {    
    struct Iterator {
        PLIST_ENTRY Current;
        PLIST_ENTRY End;

        Iterator(PLIST_ENTRY Current, PLIST_ENTRY End) : Current(Current), End(End){
        }
        
        bool Next(DataItem** item) {
            if (Current == End) {
                return false;
            }
            Current = Current->Flink;
            *item = &ToDataItem(Current);
            return true;
        }
    };

    LIST_ENTRY Head;
    int Count;

    DataItemList() {
        InitializeListHead(&Head);
    }

    void InsertTail(DataItem& item) {
        InsertTailList(&Head, &item.Entry);
        Count++;
    }

    void InsertHead(DataItem& item) {
        InsertHeadList(&Head, &item.Entry);
        Count++;
    }

    void RemoveItem(DataItem& item) {
        RemoveEntryList(&item.Entry);
        Count--;
    }

    bool IsEmpty() const {
        return Count == 0;
    }
    
    
    DataItem& PopHead() {
        PLIST_ENTRY headEntry = RemoveHeadList(&Head);
        return ToDataItem(headEntry);
    }

    DataItem& PopTail() {
        PLIST_ENTRY tailEntry = RemoveTailList(&Head);
        return ToDataItem(tailEntry);
    }

    Iterator CreateIterator() {
        return Iterator(&Head, Head.Blink);
    }

};

int main() {
    DataItem item0 = 0;
    DataItem item1(1);
    DataItem item2(2);
    DataItem item3(3);

    DataItemList list;
    list.InsertTail(item1);
    list.InsertTail(item2);
    list.InsertTail(item3);
    list.InsertHead(item0);

    DataItem* itemPtr = NULL;

    for (auto iter = list.CreateIterator(); iter.Next(&itemPtr);) {
        DataItem& item = *itemPtr;
        std::cout << item.Value << std::endl;
    }

    return 0;
}