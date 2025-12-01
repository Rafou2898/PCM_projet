//
//  queue.hpp
//  
//  Copyright (c) 2023 Marcelo Pasin. All rights reserved.
//

#include <iostream>

#ifndef _queue_hpp
#define _queue_hpp

#include "atomicstamped.hpp"

class EmptyQueueException
{
private:
	std::string _message;
public:
	EmptyQueueException(const std::string& msg) { this->_message = msg; }
	std::string message() const { return _message; }
};


template <class T>
class Queue {
private:
	template <class U>
	struct Node
	{
		U _value;
		AtomicStamped<Node<U> > _nextref;
		Node(U v) : _nextref(nullptr, 0) { this->_value = v; }
	};

	AtomicStamped<Node<T> > _headref;
	AtomicStamped<Node<T> > _tailref;

public:

	Queue()
	{
		T value;
		Node<T>* node = new Node<T>(value);
		this->_headref.set(node, 0);
		this->_tailref.set(node, 0);
	}

	void enqueue(T value)
	{
		Node<T>* node = new Node<T>(value);
		uint64_t tailStamp, nextStamp, stamp;

		while (true) {
			Node<T>* tail = this->_tailref.get(tailStamp);
			Node<T>* next = tail->_nextref.get(nextStamp);
			if (tail == this->_tailref.get(stamp) && stamp == tailStamp) {
				if (next == nullptr) {
					if (tail->_nextref.cas(next, node, nextStamp, nextStamp+1)) {
						this->_tailref.cas(tail, node, tailStamp, tailStamp+1);
						return;
					}
				} else {
					this->_tailref.cas(tail, next, tailStamp, tailStamp+1);
				}
			}
		}
	}

	T dequeue()
	{
		uint64_t tailStamp, headStamp, nextStamp, stamp;

		while (true) {
			Node<T>* head = this->_headref.get(headStamp);
			Node<T>* tail = this->_tailref.get(tailStamp);
			Node<T>* next = head->_nextref.get(nextStamp);
			if (head == this->_headref.get(stamp) && stamp == headStamp) {
				if (head == tail) {
					if (next == nullptr)
						throw(EmptyQueueException("Cannot dequeue from an empty queue."));
					this->_tailref.cas(tail, next, tailStamp, tailStamp+1);
				} else {
					T value = next->_value;
					if (this->_headref.cas(head, next, headStamp, headStamp+1)) {
						delete head;
						return value;
					}
				}
			}
		}
	}	
};






#endif // _queue_hpp