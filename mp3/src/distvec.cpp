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
			line_ss >> src_id >> dst_id;
			getline(line_ss, content);
			message msg(src_id, dst_id, content.substr(1));
			msgs.push_back(msg);
		}
	}
}

bool checkExist(int from_node_id, int to_node_id) {
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

void FillTopoWithDefaultValue() {
	for (auto it_i = node_set.begin(); it_i != node_set.end(); it_i++) {
		for (auto it_j = node_set.begin(); it_j != node_set.end(); it_j++) {
			if (*it_i == *it_j) {
				topo[*it_i][*it_j] = 0;
			}
			else {
				auto exist = checkExist(*it_i, *it_j);
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
				forward_table[src_id][dst_id] = make_pair(dst_id, topo[src_id][dst_id]);
			}
			else
			{
				forward_table[src_id][dst_id] = make_pair(UNREACHABLE, UNREACHABLE);
			}
		}
	}
}

void CalculateOnePointToOnePointDistanceVector(int src_id, int dst_id, int& min_id, int& min_cost) {
	for (auto intermed_node_id_it = node_set.begin(); intermed_node_id_it != node_set.end(); intermed_node_id_it++)
	{
		// traversal all ways from src_id to dst_id
		auto intermed_node_id = *intermed_node_id_it;
		if (topo[src_id][intermed_node_id] == UNREACHABLE ||
			forward_table[intermed_node_id][dst_id].second == UNREACHABLE) {
			// cannot reach, just pass
			continue;
		}
		else {
			auto intermed_cost = topo[src_id][intermed_node_id] + forward_table[intermed_node_id][dst_id].second;
			if ((intermed_cost > 0 && min_cost > 0 && intermed_cost < min_cost) ||
				(intermed_cost > 0 && min_cost < 0))
			{
				min_id = intermed_node_id;
				min_cost = intermed_cost;
			}
		}
	}
}

void OutputForwardTable() {
	for (auto src_id_it = node_set.begin(); src_id_it != node_set.end(); src_id_it++)
	{
		auto src_id = *src_id_it;
		for (auto dst_id_it = node_set.begin(); dst_id_it != node_set.end(); dst_id_it++)
		{
			auto dst_id = *dst_id_it;
			fpOut << dst_id << " " << forward_table[src_id][dst_id].first << " " << forward_table[dst_id][src_id].second << endl;
		}
	}
}

void CalculateOneRoundDistanceVector() {
	for (auto src_id_it = node_set.begin(); src_id_it != node_set.end(); src_id_it++)
	{
		// change src id
		auto src_id = *src_id_it;
		for (auto dst_id_it = node_set.begin(); dst_id_it != node_set.end(); dst_id_it++)
		{
			// change dst id

			auto dst_id = *dst_id_it;
			auto min_id = forward_table[src_id][dst_id].first;
			auto min_cost = forward_table[src_id][dst_id].second;

			// inplace call, find shortest path from src to dst 
			CalculateOnePointToOnePointDistanceVector(src_id, dst_id, min_id, min_cost);
			// update forward table
			forward_table[src_id][dst_id] = make_pair(min_id, min_cost);
		}
	}
}

void CalculateForwardTableDistanceVector() {
	auto node_cnt = node_set.size();
	for (auto i = 0; i < node_cnt; i++) {
		// there are node_cnt nodes, will converge at most node_cnt round
		CalculateOneRoundDistanceVector();
	}
	OutputForwardTable();
}

void SendMessage()
{
	int src_id, dst_id, temp_id;
	for (int i = 0; i < msgs.size(); i++)
	{
		src_id = msgs[i].src_id;
		dst_id = msgs[i].dst_id;
		temp_id = src_id;

		fpOut << "from " << src_id << " to " << dst_id << " cost ";
		if (forward_table[src_id][dst_id].second < 0)
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
			while (temp_id != dst_id)
			{
				fpOut << temp_id << " ";
				temp_id = forward_table[temp_id][dst_id].first;
			}
		}
		fpOut << "message " << msgs[i].content << endl;
	}
	fpOut << endl;
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
		CalculateForwardTableDistanceVector();
		SendMessage();
	}
}

int main(int argc, char** argv)
{
	if (argc != 4)
	{
		printf("Usage: ./distvec topofile messagefile changesfile\n");
		return -1;
	}

	string topo_file(argv[1]);
	string message_file(argv[2]);
	string changes_file(argv[3]);

	InitPathWeightData(topo_file);
	InitForwardTable();
	InitMessageData(message_file);
	CalculateForwardTableDistanceVector();
	SendMessage();

	UpdatePathWeightData(changes_file);
}
