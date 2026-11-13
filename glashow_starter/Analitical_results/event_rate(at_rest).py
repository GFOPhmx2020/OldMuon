import numpy as np
import matplotlib.pyplot as plt

# ===== Parameters to edit =====
E_min_PeV, E_max_PeV = 1.0, 10.0  # neutrino energy window
n_points = 10000
volume_m3 = 1.0e3                   # (10 m)^3 of rock
rho_kg_m3 = 2.16 * 1000             # Halite density (kg/m^3)
Z = 28                             # electrons per NaCl molecule
molar_mass_kg = 0.0585            # NaCl molar mass (kg/mol)
T_years = 1.0e9                # 10e9 years
anti_fraction = 1 / 4.5            # anti-nu_e / (nu_e + anti-nu_e) or 1 / 2     https://doi.org/10.1038/s41586-021-03256-1
solid_angle_sr = 4.0 * np.pi       # isotropic flux, no Earth attenuation
M_W, Gamma_W = 80.4, 2.1           # GeV
B_e, B_had = 0.11, 0.67
m_e = 0.00051099895                # GeV

# ===== Flux and resonant cross section =====
E = np.geomspace(E_min_PeV * 1e6, E_max_PeV * 1e6, n_points)  # GeV
phi_1flavor = 1.77e-18 * (E / 1e5)**(-2.74)  # GeV^-1 cm^-2 s^-1 sr^-1
phi = phi_1flavor * anti_fraction * solid_angle_sr * 1e4  # GeV^-1 m^-2 s^-1

s = m_e**2 + 2 * m_e * E                           # GeV^2 (electron at rest)
sigma_GeV2 = (24*np.pi * Gamma_W**2 * B_e * B_had
              * (s / M_W**2) / ((s-M_W**2)**2 + (M_W*Gamma_W)**2))
sigma = sigma_GeV2 * 0.389379e-31                 # GeV^(-2) -> m^2

# ===== Target electrons and event count =====
N_e = Z * 6.02214076e23 * rho_kg_m3 * volume_m3 / molar_mass_kg
rate_per_s = N_e * np.trapezoid(phi * sigma, E)
events = rate_per_s * T_years * 365.25 * 24 * 3600
E_res_PeV = ((M_W**2 - m_e**2)/(2*m_e))/1e6
print(f"Resonance energy: {E_res_PeV:.3f} PeV")
print(f"Target electrons: {N_e:.4e}")
print(f"Peak hadronic cross section: {sigma[np.argmin(abs(E/1e6-E_res_PeV))]:.3e} m^2")
print(f"Hadronic event rate: {rate_per_s:.4e} events/s")
print(f"Expected events in {T_years:g} year(s): {events:.4f}")

# ===== Flux vs. energy =====
plt.figure(figsize=(7, 4.5))
plt.loglog(E/1e6, phi, lw=2, label=r'$\bar\nu_e$ flux (all-sky)')
plt.axvline(E_res_PeV, ls='--', color='gray', label='Glashow resonance')
plt.xlabel('Neutrino energy E (PeV)')
plt.ylabel(r'$\Phi_{\bar\nu_e}$ (GeV$^{-1}$ m$^{-2}$ s$^{-1}$)')
plt.legend()
plt.grid(True, which='both', alpha=0.25)
plt.tight_layout()
plt.savefig('glashow_flux.png', dpi=180)
plt.show()
