/*
 *      List.cpp
 *      
 *      Copyright (C) 2008 Universitat Politècnica de Catalunya
 *      @author Albert Bifet - Ricard Gavaldà (abifet,gavalda@lsi.upc.edu)
 *     
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include "List.h"

////////////////////////////////////////////////////////////////////////////////

List::List(int _M):M(_M)
{
  // post: initializes the list to be empty.  
  head = nullptr;
  tail = nullptr;
  count = 0;
  addToHead();  
}

////////////////////////////////////////////////////////////////////////////////

List::~List()
{
  while (head != nullptr) removeFromHead();
}

////////////////////////////////////////////////////////////////////////////////  
     	
void List::addToHead() 
{
  //	 pre: anObject is non-null
  //	 post: the object is added to the beginning of the list

  auto *temp = new ListNode(M);

  if (head) {
    temp->next = head;
    head->prev = temp;
  }
  head = temp; 
  if (!tail)  tail = head;
  count++;
}

////////////////////////////////////////////////////////////////////////////////

void List::addToTail()
{
  //			 pre: anObject is non-null
  //			 post: the object is added at the end of the list

  auto *temp = new ListNode(M);
  if (tail) {
    temp->prev = tail;
    tail->next = temp;
  }
  tail = temp;
  if (!head) head = tail;
  count++;
}

////////////////////////////////////////////////////////////////////////////////	

void List::removeFromHead()
{
  //		 pre: list is not empty
  //		 post: removes and returns first object from the list
  ListNode *temp = head;
  head = head->next;
  if (head != nullptr)
    head->prev=nullptr;
  else
    tail = nullptr;
  count--;
  delete temp;
}
////////////////////////////////////////////////////////////////////////////////

void List::removeFromTail()
{
  //			 pre: list is not empty
  //			 post: the last object in the list is removed and returned
  ListNode *temp = tail;
  tail = tail->prev;
  if (tail == nullptr)
    head = nullptr;
  else
    tail->next=nullptr;
  count--;
  delete temp;
}
