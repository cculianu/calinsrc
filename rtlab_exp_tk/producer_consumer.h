/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001 David Christini, Lyuba Golub, Calin Culianu
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see COPYRIGHT file); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA, or go to their website at
 * http://www.gnu.org.
 */

#ifndef _PRODUCER_CONSUMER_H
#  define _PRODUCER_CONSUMER_H

#include <algorithm>
#include <set>
#include "common.h"


/*
  Producer-consumer implementation, by Calin Culianu

  This header file is a set of inline classes that basically
  are fast implementations of a consumer-producer pattern. They are faster
  than Qt's signal-slot mechanism.  This is intended to be used with the graph
  classes and SampleConsumers.
*/

/*
  A two-way node is really the parent of all producers and consumers, 
  since each connection on a node is two-way.
*/
class TwoWayNode
{
 public:
  uint add (TwoWayNode *r) 
    { r->_l.insert(this); _l.insert(r); return _l.size(); };
  uint add (TwoWayNode &r) { return add(&r); };

  uint remove(TwoWayNode *r) 
    { r->_l.erase(this); _l.erase(r); return _l.size(); };
  uint remove(TwoWayNode &r) { return(remove(&r)); };

  uint removeAll() 
    { for_each(_l.begin(), _l.end(), Remover(this)); return _l.size(); };
  
  bool connected(TwoWayNode * r) const 
    { return (_l.find(r) != _l.end()); };
  bool connected(TwoWayNode &r) const { return connected(&r); };

  template <class Type> const Type * findByType(const Type * type) const
    { type++; return for_each(_l.begin(), _l.end(), TypeFinder<Type>()).found_ref; };

  uint numConnections() const { return _l.size(); }

 private:

  class Remover
  {
   public:
    TwoWayNode *t;
    Remover(TwoWayNode *t) :  t(t) {};
    void operator () (TwoWayNode * target) { t->remove(target); };
    
  };

  template <class Type> class TypeFinder 
  {
  public:
    const Type * found_ref;
    TypeFinder() : found_ref(0) {};
    void operator () (const TwoWayNode *item) 
      { if (!found_ref) found_ref = dynamic_cast<const Type *>(item); };
  };

 protected:
  /* tell everyone that I'm dead, that way they can de-register me */
  virtual ~TwoWayNode() 
    { removeAll(); }

  set <TwoWayNode *> _l;  
};


/*
  The Consumer.  

  This consumer class may not be needed.  We could have just had another
  type for the Producer template class below, so that we wouldn't have had
  to mess around with our object hierarchy too much.  However, 
  calling the right method on the consumer when a new object was ready
  would have gotten a bit hairy. (I am not even sure how to elegantly
  handle it... )

  This is a pure interface class that basically has one method, consume().
  consume() is called by the producer whenever a new thing is ready. 
*/
template <class T> class Consumer : public TwoWayNode
{  
 public:
  virtual void consume(T) = 0;
  

 protected:
  virtual ~Consumer() { };
};


/*
  The Producer.

  Whenever a new T is ready, call produce(T).  A for_each is done
  on each of the consumers in the consumer list, and consume(T) is called.

  add, remove, and exists operate on the list of registered consumers for 
  this producer.
*/
template <class T> class Producer : public TwoWayNode
{
 private:
  /* tells the consumer about T being ready by calling 
     Consumer<T>::consume(T) */
  class TellConsumer 
  { 
    public:
      T thing;
      uint count;
      TellConsumer(T t): thing(t), count(0) {};
      void operator() (TwoWayNode * r) 
	{ 
	  Consumer<T> *c;
	  if ( ( c = dynamic_cast<Consumer <T> *>(r)))
	    c->consume(thing); count++; 
	};

  };

 public:  

  /* calls consume(thing) on all the consumers in the consumer list for
     thing */
  uint produce(T thing) 
    { return for_each(_l.begin(),_l.end(),TellConsumer(thing)).count; };
	
};

#endif
