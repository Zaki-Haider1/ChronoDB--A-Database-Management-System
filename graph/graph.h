#ifndef GRAPH_H
#define GRAPH_H

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <queue>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

class Graph {
private:
  // Adjacency List: Node -> Vector of pairs (Neighbor, Weight)
  std::unordered_map<std::string, std::vector<std::pair<std::string, int>>>adjacencyList;

public:
  // Constructor
  Graph() = default;

  // Core Graph Operations
  void addVertex(const std::string &name);
  void addEdge(const std::string &u, const std::string &v, int weight,bool isDirected);
  void printGraph() const;

  // Algorithms
  void bfs(const std::string &startNode);
  void dfs(const std::string &startNode);
  void dijkstra(const std::string &startNode, const std::string &endNode);

  // Helpers
  void removeVertex(const std::string &name);
  void removeEdge(const std::string &u, const std::string &v, bool isDirected = false);
  Graph getCopy() const; 
  void restoreFrom(const Graph& snapshot);

  // Visualization Getter
  const std::unordered_map<std::string, std::vector<std::pair<std::string, int>>>& getAdjacencyList() const;
};

class GraphEngine {
private:
  // Map: Graph Name -> Graph Object
  std::unordered_map<std::string, Graph> graphs;

public:
  // Engine Operations
  void createGraph(const std::string &name);
  Graph *getGraph(const std::string &name);
  void deleteGraph(const std::string &name);
};

#endif // GRAPH_H