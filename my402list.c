#include "my402list.h"

#include <stdlib.h>

int  My402ListLength(My402List* my402list)
{
	return my402list->num_members;
}

int  My402ListEmpty(My402List* my402list)
{
	if((my402list->num_members)==0)
		return TRUE;
	else
		return FALSE;
}

int  My402ListAppend(My402List* my402list, void* objt)
{
    My402ListElem *newitem=NULL, *last=NULL;

    newitem = (My402ListElem *)malloc(sizeof(My402ListElem));

    last = My402ListLast(my402list);

    if(newitem == NULL)
	{
        return FALSE;
    }
	else
	{
        newitem->obj = objt;
        if(My402ListEmpty(my402list) == TRUE)
		{
            newitem->prev = &my402list->anchor;
            my402list->anchor.prev = newitem;

            newitem->next = &my402list->anchor;
            my402list->anchor.next = newitem;
                       
        }
		else if(My402ListEmpty(my402list) == FALSE)
		{
            newitem->prev = last;
            last->next = newitem;
   
            newitem->next = &my402list->anchor;
            my402list->anchor.prev = newitem;
        }   
    }
    (my402list->num_members)=(my402list->num_members)+1;   
    return TRUE;   
}


int  My402ListPrepend(My402List* my402list, void* objt)
{
	My402ListElem *newitem=NULL, *first=NULL;
	
	newitem=(My402ListElem *)malloc(sizeof(My402ListElem));

	if(newitem==NULL)
	{
		return FALSE;
	}
	else
	{
		(newitem->obj)=objt;
		if(My402ListEmpty(my402list)==TRUE)
		{
			(newitem->next)=&(my402list->anchor);
			(newitem->prev)=&(my402list->anchor);
			(my402list->anchor.next)=newitem;
			(my402list->anchor.prev)=newitem;
		}
		else
		{
			first=My402ListFirst(my402list);			
			(first->prev)=newitem;
			(newitem->prev)=&(my402list->anchor);
			(my402list->anchor.next)=newitem;
			(newitem->next)=first;
		}
		(my402list->num_members)=(my402list->num_members)+1;	
		return TRUE;
	}
}

void My402ListUnlink(My402List* my402list, My402ListElem* elem)
{
	if(My402ListEmpty(my402list)==FALSE)
	{	
		My402ListElem *nextelem=NULL, *prevelem=NULL;
		nextelem=(elem->next);
		prevelem=(elem->prev);
		(nextelem->prev)=prevelem;
		(prevelem->next)=nextelem;
		(my402list->num_members)=(my402list->num_members)-1;
		free(elem);
	}
}

void My402ListUnlinkAll(My402List* my402list)
{
	if(My402ListEmpty(my402list)==FALSE)
	{
		My402ListElem *elem=NULL, *etmp=NULL;	
		elem=My402ListFirst(my402list);	
		while(elem!=NULL)
		{
			etmp=My402ListNext(my402list, elem);
			free(elem);
			elem=etmp;
		}
		(my402list->num_members)=0;
	}
}

int  My402ListInsertAfter(My402List* my402list, void* objt, My402ListElem* elem)
{
	if(elem==NULL)
	{
		return My402ListAppend(my402list, objt);
	}
	else
	{
		My402ListElem *newitem=(My402ListElem *)malloc(sizeof(My402ListElem));
		if(newitem==NULL)
		{
			return FALSE;
		}		
		else
		{
			(newitem->obj)=objt;		
			My402ListElem *nextelem=(elem->next);

			(elem->next)=newitem;
			(newitem->next)=nextelem;
			(nextelem->prev)=newitem;
			(newitem->prev)=elem;
			
			(my402list->num_members)=(my402list->num_members)+1;

			return TRUE;
		}
	}
}

int  My402ListInsertBefore(My402List* my402list, void* objt, My402ListElem* elem)
{
	if(elem==NULL)
	{
		return My402ListPrepend(my402list, objt);
	}
	else
	{
		My402ListElem *newitem=(My402ListElem *)malloc(sizeof(My402ListElem));
		if(newitem==NULL)
		{
			return FALSE;
		}		
		else
		{
			(newitem->obj)=objt;		
			My402ListElem *prevelem=(elem->prev);

			(prevelem->next)=newitem;
			(newitem->next)=elem;
			(elem->prev)=newitem;
			(newitem->prev)=prevelem;
			
			(my402list->num_members)=(my402list->num_members)+1;

			return TRUE;
		}
	}
}


My402ListElem *My402ListFirst(My402List* my402list)
{
	if(My402ListEmpty(my402list))
		return NULL;
	else
		return my402list->anchor.next;
}

My402ListElem *My402ListLast(My402List* my402list)
{
	if(My402ListEmpty(my402list))
		return NULL;
	else
		return my402list->anchor.prev;
}

My402ListElem *My402ListNext(My402List* my402list, My402ListElem* elem)
{
	if(elem==My402ListLast(my402list))
		return NULL;
	else
		return elem->next;
}

My402ListElem *My402ListPrev(My402List* my402list, My402ListElem* elem)
{
	if(elem==My402ListFirst(my402list))
		return NULL;
	else
		return elem->prev;
}


My402ListElem *My402ListFind(My402List* my402list, void* objt)
{
	My402ListElem *elem=NULL, *etmp=NULL;
	elem=My402ListFirst(my402list);
	while(elem!=NULL)
	{
		etmp=(elem->next);
		if((elem->obj)==objt)
			return elem;
		elem=etmp;
	}
	return NULL;
}

int My402ListInit(My402List* my402list)
{
	if(my402list==NULL)
		return FALSE;
	else
	{
		my402list->anchor.prev=&(my402list->anchor);
		my402list->anchor.next=&(my402list->anchor);
		my402list->anchor.obj=NULL;

		my402list->num_members=0;
		return TRUE;
	}
}
