#include<iostream>
#include<map>
#include<set>
#include<vector>
#include<sstream>
#include<string>
#include<fstream>
#include<limits.h>

#define UNREACHABLE -999
using namespace std;

map<int, map<int, pair<int, int>>>forward_table;
// key1 : source id value1: (key2 : dest id, value2: (next_hop_id, cost))
map<int, map<int, int>>topo;
// the topo
set<int>node_set;
// store all nodes in the network

typedef struct message {
	int src_id;
	int dst_id;
	string content;
	message(int src_id, int dst_id, string content) : src_id(src_id), dst_id(dst_id), content(content) {}
}message;

vector<message> msgs;

// output file
ofstream fpOut("output.txt");

void InitMessageData(string input_file_name) {
	ifstream message_file(input_file_name);

	string line, content;
	int src_id, dst_id;
	while (getline(message_file, line))
	{
		if (line != "")
		{
			stringstream line_ss(line);
			string content;
			line_ss >> src_id >> dst_id;
			getline(line_ss, content);
			message msg(src_id, dst_id, content.substr(1));
			msgs.push_back(msg);
		}
	}
}

bool checkExistInTopo(int from_node_id, int to_node_id) {
	auto it = topo.find(from_node_id);
	if (it == topo.end()) {
		return false;
	}
	auto value_map = it->second;
	auto v_it = value_map.find(to_node_id);
	if (v_it == value_map.end()) {
		return false;
	}
	else {
		return true;
	}
}

bool checkExistInForwardTable(int from_node_id, int to_node_id) {
	auto it = forward_table.find(from_node_id);
	if (it == forward_table.end()) {
		return false;
	}
	auto value_map = it->second;
	auto v_it = value_map.find(to_node_id);
	if (v_it == value_map.end()) {
		return false;
	}
	else {
		return true;
	}
}

void FillTopoWithDefaultValue() {
	for (auto it_i = node_set.begin(); it_i != node_set.end(); it_i++) {
		for (auto it_j = node_set.begin(); it_j != node_set.end(); it_j++) {
			if (*it_i == *it_j) {
				topo[*it_i][*it_j] = 0;
			}
			else {
				auto exist = checkExistInTopo(*it_i, *it_j);
				if (!exist) {
					topo[*it_i][*it_j] = UNREACHABLE;
				}
			}
		}
	}
}

void InitPathWeightData(string input_file_name) {
	ifstream file_stream(input_file_name);
	int src_id, dst_id, cost;

	// initialize graph
	while (file_stream >> src_id >> dst_id >> cost)
	{
		topo[src_id][dst_id] = cost;
		topo[dst_id][src_id] = cost;
		node_set.insert(src_id);
		node_set.insert(dst_id);
	}

	FillTopoWithDefaultValue();
}

void updateForwardTableForOneNode(int src_node_id, map<int, pair<int, int>>& cost_table) {
	auto target_forward_table_map_ptr = &(forward_table[src_node_id]);
	for (auto it = node_set.begin(); it != node_set.end(); it++) {
		auto dst_id = *it;
		auto path_value = cost_table[dst_id].second;
		int precursor_id = cost_table[dst_id].first;
		int current_node_id = dst_id;
		if (path_value >= 0) {	// can reach
			// revese lookup
			precursor_id = cost_table[dst_id].first;
			current_node_id = dst_id;
			while (precursor_id != src_node_id) {
				current_node_id = precursor_id;
				precursor_id = cost_table[precursor_id].first;
			}
		}
		(*target_forward_table_map_ptr)[dst_id] = make_pair(current_node_id, path_value);
		if(path_value != UNREACHABLE){
			fpOut << dst_id << " " << forward_table[src_node_id][dst_id].first << " " << forward_table[src_node_id][dst_id].second << endl;
		}
	}
}

void CalculateMinCostPointToOthersDijkstra(int min_cost_node_id,
	map<int, pair<int, int>>& cost_table, map<int, bool>& visited_map) {
	for (auto dst_node_it = node_set.begin(); dst_node_it != node_set.end(); dst_node_it++) {
		if (visited_map[*dst_node_it] == true) {
			continue;
		}
		int dst_id;
		dst_id = *dst_node_it;
		int old_value = cost_table[dst_id].second;

		int min_node_src_node_path_value = cost_table[min_cost_node_id].second;
		int min_node_dst_node_path_value = topo[min_cost_node_id][dst_id];
		int new_value;
		if (min_node_src_node_path_value == UNREACHABLE || min_node_dst_node_path_value == UNREACHABLE) {
			new_value = UNREACHABLE;
			// cannot reach
		}
		else {
			new_value = min_node_src_node_path_value + min_node_dst_node_path_value;
		}

		if ((new_value > 0 && old_value > 0 && new_value < old_value) || (new_value > 0 && old_value < 0)) {
			// current is smaller
			cost_table[dst_id].second = new_value;
			cost_table[dst_id].first = min_cost_node_id;
		}
		else if (new_value == old_value) {
				auto old_precusor = cost_table[dst_id].first;
				auto new_precusor = min_cost_node_id;
				if (new_precusor < old_precusor && new_precusor != dst_id) {
					cost_table[dst_id].first = new_precusor;
				}
		}
	}
}

void GetSmallestPathValueNodeID(int& min_cost, int& min_cost_node_id, map<int, pair<int, int>>& cost_table, map<int, bool>& visited_map) {
	for (auto it = cost_table.begin(); it != cost_table.end(); it++) {
		if (visited_map[it->first] == true) {
			// already in , just pass
			continue;
		}
		else {
			auto current_node = it->first;
			auto precusor_path_value_pair = it->second;
			auto path_value = precusor_path_value_pair.second;
			if (path_value != UNREACHABLE && path_value < min_cost) {
				min_cost_node_id = current_node;
				min_cost = path_value;
			}
		}
	}
}

void CalculateOneNodeToOthersDijkstra(int node_id) {
	int node_cnt = node_set.size();
	map<int, bool> visited_map;

	for (auto node : node_set) {
		visited_map[node] = false;
	}

	// init self cost
	map<int, pair<int, int>>cost_table;
	for (auto it = node_set.begin(); it != node_set.end(); it++) {
		cost_table[*it] = make_pair(node_id, topo[node_id][*it]);
	}
	visited_map[node_id] = true;

	int min_cost_node_id = node_id;
	int min_cost = 0;
	for (int cnt = 1; cnt < node_cnt; cnt++) {
		CalculateMinCostPointToOthersDijkstra(min_cost_node_id, cost_table, visited_map);
		visited_map[min_cost_node_id] = true;

		// find current path value is the smallest
		min_cost = INT_MAX;
		min_cost_node_id = -1;
		// inplace call
		GetSmallestPathValueNodeID(min_cost, min_cost_node_id, cost_table, visited_map);
		// none of nodes can reach just break
	}
	updateForwardTableForOneNode(node_id, cost_table);
}

void CalculateForwardTableDijkstra() {
	for (auto it = node_set.begin(); it != node_set.end(); it++) {
		CalculateOneNodeToOthersDijkstra(*it);
	}
}

void SendMessage()
{
	int src_id, dst_id, next_id;
	for (int i = 0; i < msgs.size(); i++)
	{
		src_id = msgs[i].src_id;
		dst_id = msgs[i].dst_id;
		next_id = src_id;

		fpOut << "from " << src_id << " to " << dst_id << " cost ";
		if (!checkExistInForwardTable(src_id, dst_id) || forward_table[src_id][dst_id].second == UNREACHABLE)
		{
			fpOut << "infinite hops unreachable ";
		}
		else if (forward_table[src_id][dst_id].second == 0)
		{
			fpOut << forward_table[src_id][dst_id].second << " hops ";
		}
		else
		{
			fpOut << forward_table[src_id][dst_id].second << " hops ";
			while (next_id != dst_id)
			{
				fpOut << next_id << " ";
				next_id = forward_table[next_id][dst_id].first;
			}
		}
		fpOut << "message " << msgs[i].content << endl;
	}
}

void InitForwardTable() {
	int src_id, dst_id;
	// traversing each node as src node to init forwarding table 
	for (auto src_id_it = node_set.begin(); src_id_it != node_set.end(); src_id_it++)
	{
		src_id = *src_id_it;
		for (auto dst_id_it = node_set.begin(); dst_id_it != node_set.end(); dst_id_it++)
		{
			dst_id = *dst_id_it;
			if (topo[src_id][dst_id] != UNREACHABLE)
			{
				forward_table[src_id][dst_id] = make_pair(src_id, topo[src_id][dst_id]);
			}
			else
			{
				forward_table[src_id][dst_id] = make_pair(src_id, UNREACHABLE);
			}
		}
	}
}

void UpdatePathWeightData(string input_file_name) {
	// change
	ifstream changes_file(input_file_name);
	int src_id, dst_id, cost;
	while (changes_file >> src_id >> dst_id >> cost)
	{
		topo[src_id][dst_id] = cost;
		topo[dst_id][src_id] = cost;
		InitForwardTable();
		CalculateForwardTableDijkstra();
		SendMessage();
	}
}

int main(int argc, char** argv)
{
	if (argc != 4)
	{
		printf("Usage: ./linkstate topofile messagefile changesfile\n");
		return -1;
	}

	string topo_file(argv[1]);
	string message_file(argv[2]);
	string changes_file(argv[3]);

	InitPathWeightData(topo_file);
	InitForwardTable();
	InitMessageData(message_file);
	CalculateForwardTableDijkstra();
	SendMessage();

	UpdatePathWeightData(changes_file);
}
