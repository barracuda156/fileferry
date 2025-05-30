#include "includes.h"
#include "List.h"
#include "Time.h"

unsigned long ListSize(ListNode *Node)
{
    ListNode *Head;

    Head=ListGetHead(Node);
    if (Head && Head->Stats) return(Head->Stats->Hits);
    return(0);
}


void MapDumpSizes(ListNode *Head)
{
    int i;
    ListNode *Chain;

    for (i=0; i < MapChainCount(Head); i++)
    {
        Chain=MapGetNthChain(Head, i);
        printf("%d %lu\n",i, ListSize(Chain));
    }
}



ListNode *ListInit(int Flags)
{
    ListNode *Node;


    Node=(ListNode *)calloc(1,sizeof(ListNode));
    Node->Head=Node;
    Node->Prev=Node;
    Node->Flags |= Flags & 0xFFFF;
//Head Node always has stats
    Node->Stats=(ListStats *) calloc(1,sizeof(ListStats));

    return(Node);
}


//A map is an array of lists (or 'chains')
//it can have up to 65534 chains
//the flag LIST_FLAG_MAP_HEAD is used to indicate the top level item
//which holds the chains as an array in it's ->Item member.
//the flag LIST_FLAG_CHAIN_HEAD is used to indicate the first item (head) of a chain
ListNode *MapCreate(int NoOfChains, int Flags)
{
    ListNode *Node, *Chains, *SubNode;
    int i;

//clear map flags out
    Flags &= ~LIST_FLAG_MAP;

    Node=ListCreate();
    Node->Flags |= LIST_FLAG_MAP_HEAD | Flags;

    if (NoOfChains > 65534) NoOfChains=65534;
    Node->ItemType=NoOfChains;

    //we allocate one more than we will use, so the last one acts as a terminator
    Chains=(ListNode *) calloc(NoOfChains+1, sizeof(ListNode));
    Node->Item=Chains;

    for (i=0; i < NoOfChains; i++)
    {
        SubNode=Chains+i;
        SubNode->ItemType=i;
        SubNode->Head=Node;
        SubNode->Prev=SubNode;
        SubNode->Flags |= LIST_FLAG_MAP_CHAIN | Flags;
        SubNode->Stats=(ListStats *) calloc(1,sizeof(ListStats));
    }

    return(Node);
}


void ListSetFlags(ListNode *List, int Flags)
{
    ListNode *Head;

    Head=ListGetHead(List);
//only the first 16bit of flags is stored. Some flags > 16 bit effect config, but
//don't need to be stored long term
    Head->Flags=Flags & 0xFFFF;
}

void ListNodeSetHits(ListNode *Node, int val)
{
    if (! Node->Stats) Node->Stats=(ListStats *) calloc(1,sizeof(ListStats));
    Node->Stats->Hits=val;
}


int ListNodeAddHits(ListNode *Node, int val)
{
    if (! Node->Stats) Node->Stats=(ListStats *) calloc(1,sizeof(ListStats));
    Node->Stats->Hits+=val;
    return(Node->Stats->Hits);
}

void ListNodeSetTime(ListNode *Node, time_t When)
{
    if (! Node->Stats) Node->Stats=(ListStats *) calloc(1,sizeof(ListStats));
    Node->Stats->Time=When;
}








ListNode *MapGetNthChain(ListNode *Map, int n)
{
    ListNode *Node;

    while (Map && Map->Head && (! (Map->Flags & LIST_FLAG_MAP_HEAD))) Map=Map->Head;
    if (Map && (Map->Flags & LIST_FLAG_MAP_HEAD))
    {
        Node=(ListNode *) Map->Item;
        return(Node + n);
    }
    return(NULL);
}


ListNode *MapGetChain(ListNode *Map, const char *Key)
{
    unsigned int i;
    ListNode *Node;

    if (Map->Flags & LIST_FLAG_MAP_HEAD)
    {
        i=fnv_hash((unsigned const char *) Key, Map->ItemType);
        Node=(ListNode *) Map->Item;
        return(Node + i);
    }
    return(NULL);
}



/*
Number of items is stored in the 'Stats->Hits' value of the head listnode. For normal nodes this would be
a counter of how many times the node has been accessed with 'ListFindNamedItem etc,
but the head node is never directly accessed this way, so we store the count of list items in this instead
*/

void ListSetNoOfItems(ListNode *LastItem, unsigned long val)
{
    ListNode *Head;

    Head=ListGetHead(LastItem);
    if (LastItem->Next==NULL) Head->Prev=LastItem; /* The head Item has its Prev as being the last item! */

    if (Head->Stats) Head->Stats->Hits=val;
}



unsigned long ListIncrNoOfItems(ListNode *List)
{
    ListNode *Head;

    if (List->Flags & LIST_FLAG_MAP_CHAIN) List->Stats->Hits++;

    Head=List->Head;
    if (Head->Flags & LIST_FLAG_MAP_CHAIN)
    {
        Head->Stats->Hits++;

        //get map head, rather than chain head
        Head=Head->Head;
    }
    Head->Stats->Hits++;

    return(Head->Stats->Hits);
}



unsigned long ListDecrNoOfItems(ListNode *List)
{
    ListNode *Head;

    if (List->Flags & LIST_FLAG_MAP_CHAIN) List->Stats->Hits--;
    Head=List->Head;
    if (Head->Flags & LIST_FLAG_MAP_CHAIN)
    {
        Head->Stats->Hits--;
        //get map head, rather than chain head
        Head=Head->Head;
    }

    Head->Stats->Hits--;

    return(Head->Stats->Hits);
}


void ListThreadNode(ListNode *Prev, ListNode *Node)
{
    ListNode *Head, *Next;

//Never thread something to itself!
    if (Prev==Node) return;

    Next=Prev->Next;
    Node->Prev=Prev;
    Prev->Next=Node;
    Node->Next=Next;

    Head=ListGetHead(Prev);
    Node->Head=Head;

// Next might be NULL! If it is, then our new node is last
// item in list, so update Head node accordingly
    if (Next) Next->Prev=Node;
    else Head->Prev=Node;

    ListIncrNoOfItems(Prev);
}


void ListUnThreadNode(ListNode *Node)
{
    ListNode *Head, *Prev, *Next;

    ListDecrNoOfItems(Node);
    Prev=Node->Prev;
    Next=Node->Next;
    if (Prev !=NULL) Prev->Next=Next;
    if (Next !=NULL) Next->Prev=Prev;

    Head=ListGetHead(Node);
    if (Head)
    {
//prev node of head points to LAST item in list
        if (Head->Prev==Node)
        {
            Head->Prev=Node->Prev;
            if (Head->Prev==Head) Head->Prev=NULL;
        }

        if (Head->Side==Node) Head->Side=NULL;
        if (Head->Next==Node) Head->Next=Next;
        if (Head->Prev==Node) Head->Prev=Prev;
    }

    //make our unthreaded node a singleton
    Node->Head=NULL;
    Node->Prev=NULL;
    Node->Next=NULL;
    Node->Side=NULL;
}



void MapClear(ListNode *Map, LIST_ITEM_DESTROY_FUNC ItemDestroyer)
{
    ListNode *Node;

    if (Map->Flags & LIST_FLAG_MAP_HEAD)
    {
        for (Node=(ListNode *) Map->Item; Node->Flags & LIST_FLAG_MAP_CHAIN; Node++) ListClear(Node, ItemDestroyer);
    }
}


void ListClear(ListNode *ListStart, LIST_ITEM_DESTROY_FUNC ItemDestroyer)
{
    ListNode *Curr,*Next;

    if (! ListStart) return;
    if (ListStart->Flags & LIST_FLAG_MAP_HEAD) MapClear(ListStart, ItemDestroyer);

    Curr=ListStart->Next;
    while (Curr)
    {
        Next=Curr->Next;
        if (ItemDestroyer && Curr->Item) ItemDestroyer(Curr->Item);
        DestroyString(Curr->Tag);
        if (Curr->Stats) free(Curr->Stats);
        free(Curr);
        Curr=Next;
    }

    ListStart->Next=NULL;
    ListStart->Side=NULL;
    ListStart->Head=ListStart;
    ListStart->Prev=ListStart;
    ListSetNoOfItems(ListStart,0);
}


void ListDestroy(ListNode *ListStart, LIST_ITEM_DESTROY_FUNC ItemDestroyer)
{
    if (! ListStart) return;
    ListClear(ListStart, ItemDestroyer);
    if (ListStart->Item) free(ListStart->Item);
    if (ListStart->Stats) free(ListStart->Stats);
    free(ListStart);
}



void ListAppendItems(ListNode *Dest, ListNode *Src, LIST_ITEM_CLONE_FUNC ItemCloner)
{
    ListNode *Curr;
    void *Item;

    Curr=ListGetNext(Src);
    while (Curr !=NULL)
    {
        if (ItemCloner)
        {
            Item=ItemCloner(Curr->Item);
            ListAddNamedItem(Dest, Curr->Tag, Item);
        }
        else ListAddNamedItem(Dest, Curr->Tag, Curr->Item);
        Curr=ListGetNext(Curr);
    }
}


ListNode *ListClone(ListNode *ListStart, LIST_ITEM_CLONE_FUNC ItemCloner)
{
    ListNode *NewList;

    NewList=ListCreate();

    ListAppendItems(NewList, ListStart, ItemCloner);
    return(NewList);
}



ListNode *ListInsertTypedItem(ListNode *InsertNode, uint16_t Type, const char *Name, void *Item)
{
    ListNode *NewNode;

    if (! InsertNode) return(NULL);
    NewNode=(ListNode *) calloc(1,sizeof(ListNode));
    ListThreadNode(InsertNode, NewNode);
    NewNode->Item=Item;
    NewNode->ItemType=Type;
    if (StrValid(Name)) NewNode->Tag=CopyStr(NewNode->Tag,Name);
    if (InsertNode->Head->Flags & LIST_FLAG_STATS)
    {
        NewNode->Stats=(ListStats *) calloc(1,sizeof(ListStats));
        NewNode->Stats->Time=GetTime(TIME_CACHED);
    }

    return(NewNode);
}


ListNode *ListAddTypedItem(ListNode *ListStart, uint16_t Type, const char *Name, void *Item)
{
    ListNode *Curr;

    if (ListStart->Flags & LIST_FLAG_MAP_HEAD) ListStart=MapGetChain(ListStart, Name);

    if (ListStart->Flags & LIST_FLAG_ORDERED) Curr=ListFindNamedItemInsert(ListStart, Name);
    else Curr=ListGetLast(ListStart);

    if (Curr==NULL) return(Curr);
    return(ListInsertTypedItem(Curr,Type,Name,Item));
}



int ListConsiderInsertPoint(ListNode *Head, ListNode *Prev, const char *Name)
{
    int result;

    if (Prev && (Prev != Head) && Prev->Tag)
    {
        if (Head->Flags & LIST_FLAG_CASE) result=strcmp(Prev->Tag,Name);
        else result=strcasecmp(Prev->Tag,Name);

        if (result == 0) return(TRUE);
        if ((Head->Flags & LIST_FLAG_ORDERED) && (result < 1)) return(TRUE);
    }

    return(FALSE);
}


ListNode *ListFindNamedItemInsert(ListNode *Root, const char *Name)
{
    ListNode *Prev=NULL, *Curr, *Next, *Head;
    int result=0;
    unsigned long long val;

    if (! Root) return(Root);
    if (! StrValid(Name)) return(Root);

    if (Root->Flags & LIST_FLAG_MAP_HEAD) Head=MapGetChain(Root, Name);
    else Head=Root;

    //Check last item in list
    if (ListConsiderInsertPoint(Head, Head->Prev, Name)) return(Head->Prev);

    Prev=Head;
    Curr=Head->Next;
    //if LIST_FLAG_CACHE is set, then the general purpose 'Side' pointer of the head node points to a cached item
    if ((Root->Flags & LIST_FLAG_CACHE) && Head->Side && Head->Side->Tag)
    {
        if (ListConsiderInsertPoint(Head, Head->Side, Name)) Curr=Head->Side;
    }

    while (Curr)
    {
        //we get 'Next' here because we might delete 'Curr' if we are operating
        //as a list with 'timeouts' (LIST_FLAG_TIMEOUT)
        Next=Curr->Next;

        if (Curr->Tag)
        {
            if (ListConsiderInsertPoint(Head, Curr, Name)) return(Curr);

            //Can only get here if it's not a match, in which
            //case we can safely delete any 'timed out' items
            if (Root->Flags & LIST_FLAG_TIMEOUT)
            {
                val=ListNodeGetTime(Curr);
                if ((val > 0) && (val < GetTime(TIME_CACHED)))
                {
                    Destroy(Curr->Item);
                    ListDeleteNode(Curr);
                }
            }
        }
        Prev=Curr;
        Curr=Next;
    }

    return(Prev);
}



ListNode *ListFindTypedItem(ListNode *Root, int Type, const char *Name)
{
    ListNode *Node, *Head;
    int result;

    if (! Root) return(NULL);
    Node=ListFindNamedItemInsert(Root, Name);

    //item must have a name, and can't be the 'head' of the list
    if ((! Node) || (Node==Node->Head) || (! Node->Tag)) return(NULL);

    //'Root' can be a Map head, rather than a list head, so we call 'ListFindNamedItemInsert' to get the correct
    //insert chain
    Head=Node->Head;

    if (Head)
    {
        while (Node)
        {
            if (Head->Flags & LIST_FLAG_CASE) result=CompareStr(Node->Tag,Name);
            else result=CompareStrNoCase(Node->Tag,Name);

            if (
                (result==0) &&
                ( (Type==ANYTYPE) || (Type==Node->ItemType) )
            )
            {
                if (Head->Flags & LIST_FLAG_CACHE) Head->Side=Node;
                if (Node->Stats) Node->Stats->Hits++;
                return(Node);
            }

            //if this is set then there's at most one instance of an item with a given name
            if (Head->Flags & LIST_FLAG_UNIQ) break;

            //if it's an ordered list and the strcmp didn't match, then give up as there will be no more matching items
            //past this point
            if ((Head->Flags & LIST_FLAG_ORDERED) && (result !=0)) break;


            Node=ListGetNext(Node);
        }
    }
    return(NULL);
}


ListNode *ListFindNamedItem(ListNode *Head, const char *Name)
{
    return(ListFindTypedItem(Head, ANYTYPE, Name));
}



ListNode *InsertItemIntoSortedList(ListNode *List, void *Item, int (*LessThanFunc)(void *, void *, void *))
{
    ListNode *Curr, *Prev;

    Prev=List;
    Curr=Prev->Next;
    while (Curr && (LessThanFunc(NULL, Curr->Item,Item)) )
    {
        Prev=Curr;
        Curr=Prev->Next;
    }

    return(ListInsertItem(Prev,Item));
}

//list get next is just a macro that either calls this for maps, or returns Node->next
ListNode *MapChainGetNext(ListNode *CurrItem)
{
    if (! CurrItem) return(NULL);

    if (CurrItem->Next)
    {
        return(CurrItem->Next);
    }

    if (CurrItem->Flags & LIST_FLAG_MAP_HEAD)
    {
        CurrItem=(ListNode *) CurrItem->Item;
        if (CurrItem->Next) return(CurrItem->Next);
    }

    return(NULL);
}


ListNode *MapGetNext(ListNode *CurrItem)
{
    ListNode *SubNode, *ChainHead;

    if (! CurrItem) return(NULL);
    SubNode=MapChainGetNext(CurrItem);
    if (SubNode) return(SubNode);

    //if the CurrItem is a MAP_HEAD, the very top of a map, then ChainHead is the
    //first chain in it's collection of chains, held in '->Item'
    if (CurrItem->Flags & LIST_FLAG_MAP_HEAD) ChainHead=(ListNode *) CurrItem->Item;
    //if the CurrItem is the head of a chain, then it's the chain head
    else if (CurrItem->Flags & LIST_FLAG_MAP_CHAIN) ChainHead=CurrItem;
    //otherwise get the chain head
    else ChainHead=ListGetHead(CurrItem);

    while (ChainHead && (ChainHead->Flags & LIST_FLAG_MAP_CHAIN))
    {
        ChainHead=MapGetNthChain(ChainHead->Head, ChainHead->ItemType +1);
        if (ChainHead->Next) return(ChainHead->Next);
    }

    return(NULL);
}



ListNode *ListGetPrev(ListNode *CurrItem)
{
    ListNode *Prev;

    if (CurrItem == NULL) return(NULL);
    Prev=CurrItem->Prev;
    /* Don't return the dummy header! */
    if (Prev && (Prev->Prev !=NULL) && (Prev != Prev->Head)) return(Prev);
    return(NULL);
}


ListNode *ListGetLast(ListNode *CurrItem)
{
    ListNode *Head;

    Head=ListGetHead(CurrItem);
    if (! Head) return(CurrItem);
    /* the dummy header has a 'Prev' entry that points to the last item! */
    if (Head->Prev) return(Head->Prev);
    return(Head);
}



ListNode *ListGetNth(ListNode *Head, int n)
{
    ListNode *Curr;
    int count=0;

    if (! Head) return(NULL);

    Curr=ListGetNext(Head);
    while (Curr && (count < n))
    {
        count++;
        Curr=ListGetNext(Curr);
    }
    if (count < n) return(NULL);
    return(Curr);
}





ListNode *ListJoin(ListNode *List1, ListNode *List2)
{
    ListNode *Curr, *StartOfList2;

    Curr=List1;
    /*Lists all have a dummy header!*/
    StartOfList2=List2->Next;

    while (Curr->Next !=NULL) Curr=Curr->Next;
    Curr->Next=StartOfList2;
    StartOfList2->Prev=Curr;

    while (Curr->Next !=NULL) Curr=Curr->Next;
    return(Curr);
}


//Item1 is before Item2!
void ListSwapItems(ListNode *Item1, ListNode *Item2)
{
    ListNode *Head, *Prev, *Next;

    if (! Item1) return;
    if (! Item2) return;

//Never swap with a list head!
    Head=ListGetHead(Item1);
    if (Head==Item1) return;
    if (Head==Item2) return;

    Prev=Item1->Prev;
    Next=Item2->Next;

    if (Head->Next==Item1) Head->Next=Item2;
    if (Head->Prev==Item1) Head->Prev=Item2;

    if (Prev) Prev->Next=Item2;
    Item1->Prev=Item2;
    Item1->Next=Next;

    if (Next) Next->Prev=Item1;
    Item2->Prev=Prev;
    Item2->Next=Item1;

}


void ListSort(ListNode *List, void *Data, int (*LessThanFunc)(void *, void *, void *))
{
    ListNode *Curr=NULL, *Prev=NULL;
    int sorted=0;

    while (! sorted)
    {
        sorted=1;
        Prev=NULL;
        Curr=ListGetNext(List);
        while (Curr)
        {
            if (Prev !=NULL)
            {
                if ( (*LessThanFunc)(Data,Curr->Item,Prev->Item) )
                {
                    sorted=0;
                    ListSwapItems(Prev,Curr);
                }
            }
            Prev=Curr;
            Curr=ListGetNext(Curr);
        }
    }

}

void ListSortNamedItems(ListNode *List)
{
    ListNode *Curr=NULL, *Prev=NULL;
    int sorted=0;

    while (! sorted)
    {
        sorted=1;
        Prev=NULL;
        Curr=ListGetNext(List);
        while (Curr)
        {
            if (Prev !=NULL)
            {
                if (CompareStr(Prev->Tag,Curr->Tag) < 0)
                {
                    sorted=0;
                    ListSwapItems(Prev,Curr);
                }
            }

            Prev=Curr;
            Curr=ListGetNext(Curr);
        }
    }

}





ListNode *ListFindItem(ListNode *Head, void *Item)
{
    ListNode *Curr;

    if (! Item) return(NULL);
    Curr=ListGetNext(Head);
    while (Curr)
    {
        if (Curr->Item==Item)
        {
            if (Head->Flags & LIST_FLAG_SELFORG) ListSwapItems(Curr->Prev, Curr);
            return(Curr);
        }
        Curr=ListGetNext(Curr);
    }
    return(Curr);
}


void *ListDeleteNode(ListNode *Node)
{
    void *Contents;

    if (Node==NULL) return(NULL);

    ListUnThreadNode(Node);
    if (Node->Stats) free(Node->Stats);
    Contents=Node->Item;
    Destroy(Node->Tag);
    free(Node);
    return(Contents);
}

