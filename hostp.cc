#include <dpu>
#include <iostream>
#include <string>
#include <unistd.h>
#include <iomanip>
#include "pgm/pgm_index.hpp"

using namespace dpu;


template <typename K>
struct Nodeinfo
{
	K lkey;
	K rkey;
	int n_segs;
	int segs_index;  // position of the first segments in PGM-index segments 
};



const std::string dpu_binary = "/home/guest/neospace/piecewisel/build/dpuprog";

/* Size of the buffer for which we compute the checksum: 64KBytes. */
static constexpr int32_t BUFFER_SIZE = 8;

/* some configures */
const int epsilon = 128;
const int maxsegs_per_dpu = 64;

std::vector<std::vector<int>> result(1);
std::vector<int> keys(BUFFER_SIZE);

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

std::vector<Nodeinfo<int>>
distirution_list(const std::vector<pgm::PGMIndex<int, epsilon>::Segment>& original, std::vector<size_t>& levels_offsets, int ndpus)
{
	int segs_per_dpu = (levels_offsets[1] / ndpus);
	std::vector<Nodeinfo<int>> infolist(ndpus, {0,0,segs_per_dpu,0});
	int i = 0;
	for(int left = (levels_offsets[1] - segs_per_dpu*ndpus); left > 0; left--)
		infolist[i++].n_segs++;

	i = 0;
	for(auto &e : infolist)
	{
		e.segs_index = i;
		i += e.n_segs;
	}

	return infolist;
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
std::vector<T> get_subset(const std::vector<T>& original, int start, int len) {
    
    if (start < 0 || start >= original.size() || len <= 0 || (start + len) >= original.size()) {
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
 * @return 0 if succeed otherwise -1
 */
int distribute_segments(dpu::DpuSet &system, pgm::PGMIndex<int, epsilon> &pgi)
{
	int ndpus = system.dpus().size();
	auto levels_offsets = pgi.get_levels_offsets();
	auto segments = pgi.get_segments();

	distirution_list(segments, levels_offsets, ndpus);
	
	int segs_per_dpu = (levels_offsets[1] / ndpus);
	segs_per_dpu = segs_per_dpu? segs_per_dpu:1;

	if(segs_per_dpu > maxsegs_per_dpu)
		return -1;

	for(int i = 0; i < (ndpus-1); i++)
	{
		auto subset = get_subset(segments, i*segs_per_dpu, segs_per_dpu);
		system.dpus()[i]->copy("segments_list", subset, static_cast<unsigned>(subset.size() * sizeof(subset[0])));
	}


	// 8字节对齐问题
	// if(levels_offsets[1] - (ndpu-1)*segs_per_dpu > 0)
	// {
	// 	auto subset = get_subset(segments, (ndpu-1)*segs_per_dpu, levels_offsets[1] - (ndpu-1)*segs_per_dpu);
	// 	auto pdata = subset.data();
	// 	system.dpus()[ndpu-1]->copy("segments_list", subset, static_cast<unsigned>(subset.size() * sizeof(subset[0])));
	// }
	return 0;
}

int main(int argc, char **argv)
{
	try
	{
		std::vector<int> data;
		generate_data(data);
		auto pgi = build_index(data);
		auto system = DpuSet::allocate(8);
		system.load(dpu_binary);
		distribute_segments(system, pgi);
		system.exec();
		system.log(std::cout);
	}
	catch (const DpuError &e)
	{
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
