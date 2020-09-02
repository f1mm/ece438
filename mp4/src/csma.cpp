#include <iostream>
#include <functional>
#include <algorithm>
#include <map>
#include <vector>
#include <sstream>
#include <fstream>
#include <string>
#include <random>
#include <ctime>
using namespace std;

class Node {
public:
	Node(int packet_length, int max_attempt_cnt, int back_off) {
		packet_length_ = packet_length;
		back_off_ = back_off;
		max_attempt_cnt_ = max_attempt_cnt;
		collision_cnt_ = 0;
	}
	int collision_cnt_;
	int packet_length_;
	int back_off_;
	int max_attempt_cnt_;
};

vector<Node>nodes;
map<int, int>success_cnt_map;
map<int, int>col_cnt_map;
long total_collision_cnt;
long channel_idle;
long channel_used;
long packet_send;
void readFromFile(string input_file_name) {
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
		}
	}
}

void init(int node_cnt, int packet_length, int max_attempt_cnt, 
	vector<int>& collision_backoff_time_vector) {
	total_collision_cnt = 0;
	channel_idle = 0;
	channel_used = 0;
	packet_send = 0;
	for (int i = 0; i < node_cnt; i++) {
		static default_random_engine e(time(nullptr));
		static uniform_int_distribution<unsigned> u(0, collision_backoff_time_vector[0]);
		int back_off = u(e);
		Node node(packet_length, max_attempt_cnt, back_off);
		nodes.push_back(node);
	}
}

vector<int>getReadyNodesIndex() {
	vector<int>finished_node_index;
	for (size_t i = 0; i < nodes.size(); i++) {
		if (nodes[i].back_off_ == 0) {
			finished_node_index.push_back(i);
		}
	}
	return finished_node_index;
}

void resetNode(int index, vector<int>& collision_backoff_time_vector, int col_cnt) {
	auto& node = nodes[index];
	static default_random_engine e(time(nullptr));
	static uniform_int_distribution<unsigned> u(0, collision_backoff_time_vector[collision_backoff_time_vector.size() - 1]);
	node.back_off_ = u(e) % collision_backoff_time_vector[col_cnt];
	node.collision_cnt_ = col_cnt;
}

void simulation(int max_attempt_cnt, long total_time, vector<int>& collision_backoff_time_vector) {
	for (long i = 0; i < total_time; i++) {
		vector<int>ready_node_index = getReadyNodesIndex();
		if (ready_node_index.size() == 0) {
			// no node will send
			channel_idle++;
			for_each(nodes.begin(), nodes.end(), [&](Node& node) {
				node.back_off_--;
				});
		}
		else if (ready_node_index.size() == 1) {
			// only one node want to send the data at this point
			auto& cur_node = nodes[ready_node_index[0]];
			if (i + cur_node.packet_length_ < total_time) {
				// the rest time can send the packet
				packet_send++;
				channel_used += cur_node.packet_length_;
				i += cur_node.packet_length_ - 1;
				resetNode(ready_node_index[0], collision_backoff_time_vector, cur_node.collision_cnt_);
				success_cnt_map[ready_node_index[0]]++;
			}
			else {
				// the rest time cannot send the packet
				channel_used += total_time - i;
				break;
			}
		}
		else if (ready_node_index.size() > 1) {
			for (size_t i = 0; i < ready_node_index.size(); i++) {
				auto& cur_node = nodes[ready_node_index[i]];
				col_cnt_map[ready_node_index[i]]++;
				if (cur_node.collision_cnt_ < max_attempt_cnt) {
					++cur_node.collision_cnt_;
					static default_random_engine e(time(nullptr));
					static uniform_int_distribution<int> u(0, collision_backoff_time_vector[collision_backoff_time_vector.size() - 1]);
					cur_node.back_off_ = u(e) % collision_backoff_time_vector[cur_node.collision_cnt_];
				}
				else {
				    resetNode(ready_node_index[i], collision_backoff_time_vector, 0);
				}
			}
			total_collision_cnt++;
		}
	}
}

double calculteVariance(map<int, int>& mp) {
	double sum = 0;
	double average = 0;
	for (auto it = mp.begin(); it != mp.end(); it++) {
		sum += it->second;
	}
	average = sum / mp.size();
	double variance_sum = 0;
	for (auto it = mp.begin(); it != mp.end(); it++) {
		variance_sum += (it->second - average) * (it->second - average);
	}
	return variance_sum / mp.size();
}

void writeToOutput(int total_time) {
	ofstream outfile;
	outfile.open("output.txt");
	outfile << "Channel utilization (in percentage) " << ((double)channel_used / total_time) * 100.0 << endl;
	outfile << "Channel idle fraction (in percentage) " << ((double)channel_idle / total_time) * 100.0 << endl;
	outfile << "Total number of collisions " << total_collision_cnt << endl;
	outfile << "Variance in number of successful transmissions (across all nodes) " << calculteVariance(success_cnt_map) << endl;
	outfile << "Variance in number of collisions (across all nodes) " << calculteVariance(col_cnt_map) << endl;
	outfile.close();
}
int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: ./csma input.txt\n");
		exit(1);
	}

	int node_cnt, packet_length, max_attempt_cnt, total_time;
	vector<int>collision_backoff_time_vector;
	char tmp;

	ifstream input_file_stream(argv[1]);
	input_file_stream >> tmp >> node_cnt >> tmp >> packet_length >> tmp;
	while (input_file_stream.get() != 10) {
		int backoff_time;
		input_file_stream >> backoff_time;
		collision_backoff_time_vector.push_back(backoff_time);
	}
    input_file_stream >> tmp >> max_attempt_cnt >> tmp >> total_time;
    init(node_cnt, packet_length, max_attempt_cnt, collision_backoff_time_vector);
	simulation(max_attempt_cnt, total_time, collision_backoff_time_vector);
	writeToOutput(total_time);
	return 0;
}