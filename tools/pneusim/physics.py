"""Closed-form lumped pneumatic formulas (SI units)."""
import numpy as np

GAMMA = 1.4            # adiabatic index of air
P0 = 101325.0          # atmospheric pressure [Pa]
MU = 1.81e-5           # dynamic viscosity of air [Pa s]
RHO = 1.2              # air density [kg/m^3]
CD = 0.6               # orifice discharge coefficient
END_CORRECTION = 1.2   # effective extra length = 1.2 d (entrance/exit)


def gas_compliance(volume_m3):
    """Adiabatic gas compliance C = V / (gamma p0) [m^3/Pa]."""
    return volume_m3 / (GAMMA * P0)


def laminar_resistance(d, length):
    """Poiseuille resistance 128 mu (L + 1.2 d) / (pi d^4) [Pa s/m^3]."""
    return 128.0 * MU * (length + END_CORRECTION * d) / (np.pi * d**4)


def orifice_coefficient(d):
    """K in dp = K Q|Q| for a sharp orifice of diameter d [Pa s^2/m^6]."""
    area = np.pi * d**2 / 4.0
    return RHO / (2.0 * CD**2 * area**2)


def flow(dp, r, k):
    """Solve dp = R Q + K Q|Q| for Q (sign follows dp).

    Uses the cancellation-free root 2 dp / (R + sqrt(R^2 + 4 K |dp|)),
    which reduces to dp / R when K = 0.
    """
    return 2.0 * dp / (r + np.sqrt(r * r + 4.0 * k * np.abs(dp)))


def conductance(q, r, k):
    """Local slope dQ/d(dp) = 1 / (R + 2 K |Q|) at flow q."""
    return 1.0 / (r + 2.0 * k * np.abs(q))
