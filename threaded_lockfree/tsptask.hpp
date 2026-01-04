#pragma once
#include <bitset>
#include <climits>

#include "tspgraph.hpp"
#include "task.hpp"

/*****************************************************************
  TSPPath class, used in TSPTask
  Should call (static) TSPPath::setup() using a TSPGraph before
  creating TSPPath objects.
  Methods:
	maximise() sets the distance of path to maximum possible
	size() gets the number of nodes in path
	full() gets the size of a path with all nodes (== graph size)
	distance() gets the distance of the path
	contains(i) determines if path contains the node i
	tail() gets node in path tail
	push(node) adds node (and respective distance) to path tail
	pop() drops node ad path tail
 *****************************************************************/

class TSPPath
{
public:
	static const int FIRST_NODE = 0;
	static const int MAX_GRAPH = 32;

private:
	static TSPGraph *_graph;
	int _node[MAX_GRAPH];
	int _size;
	int _distance;
	uint32_t _content_mask;

	static int _graph_size;

public:
	static void setup(TSPGraph *graph)
	{
		_graph = graph;
		_graph_size = graph->size();
		if (_graph->size() > MAX_GRAPH)
			throw std::runtime_error("Graph bigger than MAX_GRAPH");
	}

	static int full() { return _graph_size;  }

	TSPPath()
	{
		_node[0] = FIRST_NODE;
		_size = 1;
		_distance = 0;

		_content_mask = 0; 
		_content_mask |= (1 << FIRST_NODE); 
	}

	void maximise() { _distance = INT_MAX; }
	int size() { return _size; }
	int distance() { return _distance; }
	
	bool contains(int i) { return (_content_mask & (1u << i)) != 0; } 
	int tail() { return _node[_size - 1]; }

	void push(int node)
	{
		if (node >= _graph_size)
			throw std::runtime_error("Node outside graph.");
		_distance += _graph->distance(tail(), node);

		_content_mask |= (1u << node);
		_node[_size++] = node;
	}

	void pop()
	{
		if (_size < 2)
			throw std::runtime_error("Empty path to pop().");
		_size--;
		int oldtail = _node[_size];
		int newtail = _node[_size - 1];
		if (oldtail != FIRST_NODE)
			_content_mask &= ~(1u << oldtail);
		_distance -= _graph->distance(newtail, oldtail);
	}

	void write(std::ostream &os) const
	{
		os << "{" << _distance << ": ";
		for (int i = 0; i < _size; i++)
		{
			if (i)
				os << ", ";
			os << _node[i];
		}
		os << "}";
	}
};

std::ostream &operator<<(std::ostream &os, const TSPPath &t)
{
	t.write(os);
	return os;
}

/*****************************************************************
  TSPTask class, extends Task
  Should call (static) TSPPath::setup() using a TSPGraph before
  creating TSPPath objects.
  Methods:
	reusealloc()/reusefree() replace new/delete (reuse tasks)
	cutoff(c) sets a cutoff size (from the end of a full path)
	result() gets the result after solve() or merge()
 *****************************************************************/
class TSPTask : public Task
{

private:
	static std::atomic<TSPPath *> _shortest;
	static thread_local std::vector<TSPTask *> _free_list;

	// this does not work with multiple threads! -> it does now
	TSPTask *reusealloc(int node)
	{
		if (_free_list.empty())
			return new TSPTask(this, node);
		TSPTask *p = _free_list.back();
		_free_list.pop_back();
		p->_path = _path;
		p->_cutoff_size = _cutoff_size;
		p->_path.push(node);
		return p;
	}

	// this does not work with multiple threads! -> it does now
	void reusefree(TSPTask *p)
	{
		_free_list.push_back(p);
	}

	TSPPath _path;
	int _cutoff_size;

	TSPTask(TSPTask *task, int node) : _path(task->_path), _cutoff_size(task->_cutoff_size)
	{
		_path.push(node);
	}

	void update_shortest(TSPPath &new_path)
	{
		int new_dist = new_path.distance();

		TSPPath *current = _shortest.load(std::memory_order_acquire);

		while (new_dist < current->distance())
		{
			TSPPath *new_shortest = new TSPPath(new_path);

			// we try a CAS to update _shortest
			if (_shortest.compare_exchange_weak(current, new_shortest,
												std::memory_order_release,
												std::memory_order_acquire))
			{
				//  we succeeded
				// We cannot free the old one because other threads might still read it
				break;
			}
			else
			{
				// but if we failed it means another thread modified _shortest
				// current now contains the new value
				delete new_shortest;
				// We loop again to check if we are still better
			}
		}
	}

public:
	TSPTask() { _cutoff_size = TSPPath::full(); }
	~TSPTask() override = default;

	int remaining(){
		return TSPPath::full() - _path.size();
	}
	
	void recycle(){
		reusefree(this);
	}

	// cutoff set, expressed as a distance from full path
	void cutoff(int c) { _cutoff_size = TSPPath::full() - c; }
	// TSPPath& result() { return _shortest; }
	TSPPath &result() { return *(_shortest.load()); }

	// Task interface implementation: split, merge, solve, write
	int split(TaskCollection *collection) override
	{
		// Small optimization: Call TSPPath::full() only once
		const int full = TSPPath::full();
		collection->clear();
		if (_path.size() >= _cutoff_size)
			return 0;

		TSPPath *current_shortest = _shortest.load(std::memory_order_acquire);
		int current_bound = current_shortest->distance();

		// We prune if we are already over the current best, it was missing from the original code
		if (_path.distance() >= current_bound)
		{
			return 0;
		}

		int count = 0;
		for (int i = 0; i < full; i++)
		{
			if (!_path.contains(i))
			{
				TSPTask *t = reusealloc(i);
				collection->push(t);
				count++;
			}
		}
		return count;
	}

	void merge(TaskCollection *collection) override
	{
		for (int p = 0; p < collection->size(); p++)
		{
			TSPTask *t = (TSPTask *)collection->pop();
			reusefree(t);
		}
	}

	void solve() override
	{

		// Small optimization: Call TSPPath::full() only once
		const int full = TSPPath::full();
		if (_path.size() == full)
		{
			_path.push(TSPPath::FIRST_NODE); // last node = first node

			// We get current shortest path atomically
			update_shortest(_path);

			_path.pop();
		}
		else
		{

			TSPPath *current_shortest = _shortest.load(std::memory_order_acquire);
			int current_bound = current_shortest->distance();

			for (int i = 0; i < full; i++)
			{
				if (!_path.contains(i))
				{
					_path.push(i);

					if (_path.distance() < current_bound)
						solve();
					_path.pop();
				}
			}
		}
	}

	void write(std::ostream &os) const override
	{
		std::cout << "Task(c=" << _cutoff_size << ')' << _path;
	}
};

TSPGraph *TSPPath::_graph;

TSPPath *initShortest()
{
	TSPPath *p = new TSPPath();
	p->maximise();
	return p;
}

std::atomic<TSPPath *> TSPTask::_shortest{initShortest()};

thread_local std::vector<TSPTask *> TSPTask::_free_list;

int TSPPath::_graph_size;   // ADDED
