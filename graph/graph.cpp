#include "graph.h"
#include <iostream>
#include <algorithm>
#include <queue>
#include <stack>
#include <limits>

using namespace std;

// --- Graph Class Implementation ---

void Graph::addVertex(const string &name) {
  if (adjacencyList.find(name) == adjacencyList.end()) {
    adjacencyList[name] = {};
  }
  // Idempotent: If it exists, do nothing (safe for Import)
}

const unordered_map<string, vector<pair<string, int>>>& Graph::getAdjacencyList() const {
  return adjacencyList;
}

void Graph::addEdge(const string &u, const string &v, int weight,
                    bool isDirected) {
  // Automatically add vertices if they don't exist
  if (adjacencyList.find(u) == adjacencyList.end()) {
    addVertex(u);
  }
  if (adjacencyList.find(v) == adjacencyList.end()) {
    addVertex(v);
  }

  // Add edge u -> v
  adjacencyList[u].push_back({v, weight});

  // If undirected, add edge v -> u
  if (!isDirected) {
    adjacencyList[v].push_back({u, weight});
  }
}

void Graph::printGraph() const {
  cout << "Graph Adjacency List:" << endl;
  for (const auto &pair : adjacencyList) {
    cout << pair.first << " -> ";
    for (const auto &neighbor : pair.second) {
      cout << "(" << neighbor.first << ", " << neighbor.second << ") ";
    }
    cout << endl;
  }
}

// --- GraphEngine Class Implementation ---

void GraphEngine::createGraph(const string &name) {
  if (graphs.find(name) == graphs.end()) {
    graphs[name] = Graph();
    cout << "Graph '" << name << "' created successfully." << endl;
  } else {
    cerr << "Graph '" << name << "' already exists." << endl;
  }
}

Graph *GraphEngine::getGraph(const string &name) {
  if (graphs.find(name) != graphs.end()) {
    return &graphs[name];
  } else {
    // cerr << "Graph '" << name << "' not found." << endl;
    return nullptr;
  }
}

void GraphEngine::deleteGraph(const string &name) {
  if (graphs.erase(name)) {
    cout << "Graph '" << name << "' deleted successfully." << endl;
  } else {
    cerr << "Graph '" << name << "' not found." << endl;
  }
}

// --- Graph Algorithms ---

void Graph::bfs(const string &startNode) {
  if (adjacencyList.find(startNode) == adjacencyList.end()) {
    cerr << "Start node '" << startNode << "' not found in graph."
              << endl;
    return;
  }

  unordered_map<string, bool> visited;
  queue<string> q;

  visited[startNode] = true;
  q.push(startNode);

  cout << "BFS Traversal: ";

  while (!q.empty()) {
    string current = q.front();
    q.pop();
    cout << current << " -> ";

    for (const auto &neighbor : adjacencyList[current]) {
      if (!visited[neighbor.first]) {
        visited[neighbor.first] = true;
        q.push(neighbor.first);
      }
    }
  }
  cout << "End" << endl;
}

void Graph::dfs(const string &startNode) {
  if (adjacencyList.find(startNode) == adjacencyList.end()) {
    cerr << "Start node '" << startNode << "' not found in graph."
              << endl;
    return;
  }

  unordered_map<string, bool> visited;
  stack<string> s;

  s.push(startNode);

  cout << "DFS Traversal: ";

  while (!s.empty()) {
    string current = s.top();
    s.pop();

    if (!visited[current]) {
      cout << current << " -> ";
      visited[current] = true;

      // Push neighbors to stack
      for (const auto &neighbor : adjacencyList[current]) {
        if (!visited[neighbor.first]) {
          s.push(neighbor.first);
        }
      }
    }
  }
  cout << "End" << endl;
}

void Graph::dijkstra(const string &startNode, const string &endNode) {
  if (adjacencyList.find(startNode) == adjacencyList.end()) {
    cerr << "Start node '" << startNode << "' not found in graph."
              << endl;
    return;
  }
  if (adjacencyList.find(endNode) == adjacencyList.end()) {
    cerr << "End node '" << endNode << "' not found in graph."
              << endl;
    return;
  }

  unordered_map<string, int> distances;
  unordered_map<string, string> parent;

  for (const auto &pair : adjacencyList) {
    distances[pair.first] = numeric_limits<int>::max();
  }
  distances[startNode] = 0;

  priority_queue<pair<int, string>,
                      vector<pair<int, string>>,
                      greater<pair<int, string>>>
      pq;

  pq.push({0, startNode});

  while (!pq.empty()) {
    int currentDist = pq.top().first;
    string current = pq.top().second;
    pq.pop();

    if (current == endNode)
      break;

    if (currentDist > distances[current])
      continue;

    for (const auto &neighbor : adjacencyList[current]) {
      string nextNode = neighbor.first;
      int weight = neighbor.second;

      if (distances[current] + weight < distances[nextNode]) {
        distances[nextNode] = distances[current] + weight;
        parent[nextNode] = current;
        pq.push({distances[nextNode], nextNode});
      }
    }
  }

  if (distances[endNode] == numeric_limits<int>::max()) {
    cout << "No path found from " << startNode << " to " << endNode
              << endl;
  } else {
    vector<string> path;
    string curr = endNode;
    while (curr != startNode) {
      path.push_back(curr);
      curr = parent[curr];
    }
    path.push_back(startNode);
    reverse(path.begin(), path.end());

    cout << "Path: ";
    for (size_t i = 0; i < path.size(); ++i) {
      cout << path[i] << (i < path.size() - 1 ? " -> " : "");
    }
    cout << "\nTotal Cost: " << distances[endNode] << endl;
  }
}

void Graph::removeVertex(const string &name) {
  adjacencyList.erase(name);
  for (auto& [vertex, neighbors] : adjacencyList) {
    neighbors.erase(
        remove_if(neighbors.begin(), neighbors.end(),
                      [&](const auto& p){ return p.first == name; }),
        neighbors.end()
    );
  }
}

void Graph::removeEdge(const string &u, const string &v, bool isDirected) {
  auto remove = [&](const string &from, const string &to) {
    auto &neighbors = adjacencyList[from];
    neighbors.erase(
        remove_if(neighbors.begin(), neighbors.end(),
                      [&](const auto& p){ return p.first == to; }),
        neighbors.end()
    );
  };
  remove(u, v);
  if (!isDirected) remove(v, u);
}

Graph Graph::getCopy() const {
  Graph g;
  g.adjacencyList = adjacencyList;
  return g;
}

void Graph::restoreFrom(const Graph& snapshot) {
  adjacencyList = snapshot.adjacencyList;
}