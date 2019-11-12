#pragma once

#include <filesystem>
#include <random>
#include <variant>
#include <optional>

#include "types.h"

namespace magneto {
	class IsingSystem {
	public:
		IsingSystem(const int j, const double T, const int L);
		IsingSystem(const int j, const LatticeDType& T, const int L);
		IsingSystem(const int j, const double T, const std::filesystem::path& input_path);
		[[nodiscard]] const LatticeType& get_lattice() const;
		[[nodiscard]] LatticeType& get_lattice_nc();
		size_t get_L() const;
      int get_J() const;
		std::optional<double> get_temp() const;

	private:
		LatticeType m_lattice;
		int m_J = 1;
		std::variant<double, LatticeDType> m_T;
   };

   /// <summary>Energy and Magnetization of the system at one point in time</summary>
   struct PhysicalMeasurement {
      double energy = 0.0;
      double magnetization = 0.0;
   };

   /// <summary>Energies and Magnetizations of many system states at one temperature</summary>
   struct PhysicalProperties {
      std::vector<PhysicalMeasurement> measurements;
      double T;
      unsigned int L;
   };

   PhysicalMeasurement operator+(const PhysicalMeasurement& a, const PhysicalMeasurement& b);
   PhysicalMeasurement operator/(const PhysicalMeasurement& a, const unsigned int d);

   PhysicalMeasurement get_properties(const IsingSystem& system);

	int get_dE(const LatticeType& grid, int i, int j);

   /// <summary>Returns normalized energy (per size)</summary>
   double get_E(const LatticeType& grid);

   /// <summary>Returns normalized absolute magnetization</summary>
   double get_m_abs(const LatticeType& grid);

	
	
}
