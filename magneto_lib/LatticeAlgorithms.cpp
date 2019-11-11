#include "LatticeAlgorithms.h"
#include "IsingSystem.h"

#include <sstream>

#include <deque>
#include "logging.h"

namespace {

   constexpr int get_exp_buffer_offset(int J) {
      // std::abs is not constexpr :(
      return J > 0 ? 8 * J : -8 * J;
   }

   std::string thread_id_to_string(const std::thread::id& id) {
      std::stringstream ss;
      ss << id;
      return ss.str();
   }

   struct RandomBufferGetter {
      RandomBufferGetter(const size_t buffer_size) : m_buffer_size(buffer_size) {};
      std::vector<double> operator()(){
         const std::string thread_id = thread_id_to_string(std::this_thread::get_id());
         unsigned int seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
         std::mt19937_64 rng(seed);
         std::uniform_real_distribution <double > dist_one(0.0, 1.0);
         std::vector<double> normal_random_vector;
         normal_random_vector.reserve(m_buffer_size);
         for (int i = 0; i < m_buffer_size; ++i)
            normal_random_vector.emplace_back(dist_one(rng));
         magneto::get_logger()->debug("get_random_buffer() done from thread {}", thread_id);
         return normal_random_vector;
      }
      size_t m_buffer_size;
   };


   struct LatticeIndexGetter {
      LatticeIndexGetter(const size_t buffer_size, const int lattice_size) : m_buffer_size(buffer_size), m_lattice_size(lattice_size) {};
      magneto::IndexPairVector operator()() {
         const std::string thread_id = thread_id_to_string(std::this_thread::get_id());
         unsigned int seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
         std::mt19937_64 rng(seed);
         std::uniform_int_distribution<> dist_lattice(0, m_lattice_size - 1);
         magneto::IndexPairVector indices;
         indices.reserve(m_buffer_size);
         for (int i = 0; i < m_buffer_size; ++i)
            indices.emplace_back(dist_lattice(rng), dist_lattice(rng));
         magneto::get_logger()->debug("get_lattice_indices() done from thread {}", thread_id);
         return indices;
      }
      size_t m_buffer_size;
      int m_lattice_size;
   };

} // namespace {}


magneto::Metropolis::Metropolis(const int J, const double T, const int L, const int max_rng_threads)
   : m_cached_exp_values(get_cached_exp_values(J, T))
   , m_J(J)
   , m_lattice_index_buffer(LatticeIndexGetter(L*L, L), max_rng_threads)
   , m_random_buffer(RandomBufferGetter(L*L), max_rng_threads)
{ }


void magneto::Metropolis::run(LatticeType& lattice){
   const int L = static_cast<int>(lattice.size());
   std::uniform_int_distribution <int> dist_grid(0, L - 1);
   std::uniform_real_distribution <double > dist_one(0.0, 1.0);
   const int buffer_offset = get_exp_buffer_offset(m_J);
   int flip_i, flip_j;
   int dE;
   for (int i = 0; i < m_random_buffer.get_buffer().size(); ++i) {
      flip_i = m_lattice_index_buffer.get_buffer()[i].first;
      flip_j = m_lattice_index_buffer.get_buffer()[i].second;
      dE = m_J * get_dE(lattice, flip_i, flip_j);
      if (dE <= 0 || (m_random_buffer.get_buffer()[i] < m_cached_exp_values[dE + buffer_offset]))
         lattice[flip_i][flip_j] *= -1;
   }

   m_random_buffer.refill();
   m_lattice_index_buffer.refill();
}


std::vector<double> magneto::get_cached_exp_values(const int J, const double T) {
   std::vector<double> exp_values;
   const int min_value = -8 * J;
   const int max_value = 8 * J;
   const int value_count = max_value - min_value + 1;
   const int buffer_offset = get_exp_buffer_offset(J);
   for (int dE = 0; dE < value_count; ++dE)
      exp_values.push_back(exp(-(dE - buffer_offset) / T));
   return exp_values;
}


magneto::SW::SW(const int J, const double T, const int L, const int max_rng_threads)
   : m_J(J)
   , m_T(T)
   , m_bond_north_buffer(RandomBufferGetter(L*L), max_rng_threads)
   , m_bond_east_buffer(RandomBufferGetter(L*L), max_rng_threads)
   , m_flip_buffer(RandomBufferGetter(L*L), max_rng_threads)
{ }


void magneto::SW::run(LatticeType& lattice){
   const double freezeProbability = 1.0 - exp(-2.0f * m_J / m_T);

   LatticeType discovered;
   LatticeType doesBondNorth;
   LatticeType doesBondEast;

   const int L = static_cast<int>(lattice.size());
   bool flipCluster;
   int x, y, nx, ny;
   discovered.assign(L, std::vector<char>(L, 0));
   doesBondNorth.assign(L, std::vector<char>(L, 0));
   doesBondEast.assign(L, std::vector<char>(L, 0));

   int counter = 0;
   for (int i = 0; i < L; ++i) {
      for (int j = 0; j < L; ++j) {
         doesBondNorth[i][j] = m_bond_north_buffer.get_buffer()[counter] < freezeProbability;
         doesBondEast[i][j] = m_bond_east_buffer.get_buffer()[counter] < freezeProbability;
         ++counter;
      }
   }

   counter = 0;
   for (int i = 0; i < L; ++i) {
      for (int j = 0; j < L; ++j) {
         if (!discovered[i][j]) {
            flipCluster = m_flip_buffer.get_buffer()[counter] < 0.5;
            std::deque<std::tuple<int, int>> deq(1, std::make_tuple(i, j));
            discovered[i][j] = 1;

            while (!deq.empty()) {
               x = std::get<0>(deq.front());
               y = std::get<1>(deq.front());

               nx = x;
               ny = (y + 1) % L;
               if (lattice[x][y] == lattice[nx][ny] && discovered[nx][ny] == 0 && doesBondNorth[x][y]) {
                  deq.push_back(std::make_tuple(nx, ny));
                  discovered[nx][ny] = 1;
               }

               nx = (x + 1) % L;
               ny = y;
               if (lattice[x][y] == lattice[nx][ny] && discovered[nx][ny] == 0 && doesBondEast[x][y]) {
                  deq.push_back(std::make_tuple(nx, ny));
                  discovered[nx][ny] = 1;
               }

               nx = x;
               ny = (y - 1 + L) % L;
               if (lattice[x][y] == lattice[nx][ny] && discovered[nx][ny] == 0 && doesBondNorth[x][ny]) {
                  deq.push_back(std::make_tuple(nx, ny));
                  discovered[nx][ny] = 1;
               }

               nx = (x - 1 + L) % L;
               ny = y;
               if (lattice[x][y] == lattice[nx][ny] && discovered[nx][ny] == 0 && doesBondEast[nx][y]) {
                  deq.push_back(std::make_tuple(nx, ny));
                  discovered[nx][ny] = 1;
               }

               if (flipCluster)
                  lattice[x][y] *= -1;
               deq.pop_front();
            }
         }
         ++counter;
      }
   }

   m_bond_north_buffer.refill();
   m_bond_east_buffer.refill();
   m_flip_buffer.refill();
}
