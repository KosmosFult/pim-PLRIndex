#include <dpu>
#include <iostream>
#include <string>
#include <unistd.h>
#include <iomanip>
#include "pgm/pgm_index.hpp"
#include "defs.h"
#include <algorithm>

using namespace dpu;

struct Nodeinfo
{
	// 最小和最大key在data中的下标
	int lkey_i;
	int rkey_i;

	int n_segs;
	int segs_index; // position of the first segments in PGM-index segments
};

const std::string dpu_binary = "/home/guest/neospace/piecewisel/build/dpuprog";

/* Size of the buffer for which we compute the checksum: 64KBytes. */
static constexpr int32_t BUFFER_SIZE = 8;

/* some configures */
const int epsilon = EPSILON;
const int maxsegs_per_dpu = 64;

std::vector<std::vector<int>> result(1);
std::vector<int> keys(BUFFER_SIZE);
std::vector<Nodeinfo> distri_list;
std::vector<int> data;

void populate_mram(DpuSetOps &dpu, std::vector<int> &keys)
{
	dpu.copy("keys", keys, static_cast<unsigned>(BUFFER_SIZE * sizeof(int)));
}

void generate_data(std::vector<int> &data, int data_size = 1000000)
{
	data.resize(data_size);
	std::generate(data.begin(), data.end(), std::rand);
	// data.push_back(42);
	std::sort(data.begin(), data.end());
	return;
}

pgm::PGMIndex<int, epsilon>
build_index(std::vector<int> &data)
{
	pgm::PGMIndex<int, epsilon> pgi(data);
	return pgi;
}

// 按segments的数量均分，可能按data分更好
std::vector<Nodeinfo>
distirution_list(const std::vector<int> &data, const std::vector<pgm::PGMIndex<int, epsilon>::Segment> &original, std::vector<size_t> &levels_offsets, int ndpus)
{
	int segs_per_dpu = (levels_offsets[1] / ndpus);
	std::vector<Nodeinfo> infolist(ndpus, {0, 0, segs_per_dpu, 0});
	int i = 0;
	for (int left = (levels_offsets[1] - segs_per_dpu * ndpus); left > 0; left--)
		infolist[i++].n_segs++;

	i = 0;			// 当前segments的下标
	int data_i = 0; // 当前
	for (auto &e : infolist)
	{
		e.segs_index = i;
		i += e.n_segs;
		e.lkey_i = data_i;

		int next_seg_lkey = (i >= levels_offsets[1]) ? INT32_MAX : original[i].key;

		// for(int j = data_i+1; j < data.size() && data[j] < next_seg_lkey; j++);
		while (data_i < data.size() && data[data_i] < next_seg_lkey)
			data_i++;

		e.rkey_i = data_i - 1;
	}

	return infolist;
}

/**
 * @brief aligned the bytes size of orginal vector for the dpu copy. It will pad elements util the size is aligned
 * by 8.
 * @tparam T the vector elements type
 * @param orginal vector to pad
 * @return 0 if success else -1
 */
template <typename T>
int aligned_size8(std::vector<T> &orginal)
{
	size_t orginal_size = orginal.size() * sizeof(T);
	if (orginal_size % 8 == 0)
		return 0;

	int n_add = 1;
	int max_add = 16;
	// 数学上有待改进
	while ((orginal_size + n_add * sizeof(T)) % 8 > 0 && n_add < max_add)
		n_add++;

	if (n_add == max_add)
	{
		std::cerr << "aligned_size8: max add" << std::endl;
		return -1;
	}

	for (; n_add > 0; n_add--)
		orginal.push_back(T());

	return 0;
}

/**
 * @brief Get the subset of vector
 * @tparam T element type
 * @param original target vector
 * @param start the start index of original to construct the subset
 * @param len the subset vector len
 * @return subset vector
 */
template <typename T>
std::vector<T> get_subset(const std::vector<T> &original, int start, int len)
{

	if (start >= original.size())
		return std::vector<T>();

	if (start < 0 || len <= 0 || (start + len) > original.size())
	{
		std::cerr << "Invalid index range." << std::endl;
		return std::vector<T>();
	}

	std::vector<T> subset(original.begin() + start, original.begin() + start + len);
	return subset;
}

/**
 * @brief Distriute the segments of PGM-index to the pim module.
 * In other words, cpoy the segments to dpus' MRAM
 * @param system the target dpu set
 * @param pgi source pgm-index
 * @param data source data
 * @return 0 if succeed otherwise -1
 */
int distribute(dpu::DpuSet &system, const pgm::PGMIndex<int, epsilon> &pgi, const std::vector<int> &data)
{
	int ndpus = system.dpus().size();
	auto levels_offsets = pgi.get_levels_offsets();
	auto segments = pgi.get_segments();

	distri_list = distirution_list(data, segments, levels_offsets, ndpus);

	int data_i = 0;
	for (int i = 0; i < ndpus; i++)
	{
		// 这里前12字节(sub_segs第一个元素存放segments的数量)
		auto sub_segs = get_subset(segments, distri_list[i].segs_index, distri_list[i].n_segs);
		auto sub_data = get_subset(data, distri_list[i].lkey_i, distri_list[i].rkey_i + 1 - distri_list[i].lkey_i);
		
		sub_segs.insert(sub_segs.begin(), {distri_list[i].n_segs, 0, distri_list[i].lkey_i});
		sub_data.insert(sub_data.begin(), sub_data.size());

		aligned_size8(sub_segs);
		aligned_size8(sub_data);
		system.dpus()[i]->copy("segments_list", sub_segs);
		system.dpus()[i]->copy("data", sub_data);
	}

	return 0;
}

template <typename T>
std::vector<T> generate_query(std::vector<T> &data, int q_size)
{
	std::vector<T> querys(q_size);
	int l = 0;
	int r = data.size();
	auto grand = [&]()
	{
		return data[rand() % (r - l + 1) + l];
	};
	std::generate(querys.begin(), querys.end(), grand);
	return querys;
}

template <typename T>
std::vector<std::vector<T>> distribute_query(std::vector<T> &querys, dpu::DpuSet &system)
{
	// 注意querys_list中的元素vector长度必须一致，因为copy中缺省size是第一个vector的size
	std::vector<std::vector<T>> querys_list(distri_list.size(), std::vector<T>(QUERY_BATCH - 1));
	std::vector<int> list_added(querys_list.size(), 0); // list_added[i]表示querys_list[i]的长度
	std::vector<T> fences;
	for (auto &e : distri_list)
		fences.push_back(data[e.lkey_i]);

	for (auto &k : querys)
	{
		auto itr = std::lower_bound(fences.begin(), fences.end(), k);
		int i = std::distance(fences.begin(), itr) - 1;
		querys_list[i][list_added[i]++] = k;
	}

	for (int i = 0; i < querys_list.size(); i++)
	{
		querys_list[i].insert(querys_list[i].begin(), list_added[i]);
		aligned_size8(querys_list[i]);
		// system.dpus()[i]->copy("query_keys", querys_list[i]);
	}
	system.copy("query_keys", querys_list);

	return querys_list;
}

// 返回下标
std::vector<int> get_result()
{
}

int main(int argc, char **argv)
{
	try
	{
		generate_data(data);
		auto pgi = build_index(data);

		auto system = DpuSet::allocate(8);
		system.load(dpu_binary);
		distribute(system, pgi, data);

		auto querys = generate_query(data, 256);
		distribute_query(querys, system);

		system.exec();

		// get_result();
		system.log(std::cout);
	}
	catch (const DpuError &e)
	{
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
