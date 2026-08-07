# SuperMini nRF52840 low-power conversion

This project uses the black nice!nano-compatible SuperMini nRF52840 with a
CR2032. The tested boards consumed about 0.4-0.8 mA in System ON idle when the
nRF52840 was powered through VDDH/REG0. Supplying VDD and VDDH together puts
the SoC in normal-voltage mode and removes that overhead.

## Hardware changes

1. Remove the faulty 5.6 kOhm pull-up associated with the P0.13 external-VCC
   control circuit.
2. Remove the two-terminal W5 power-path diode.
3. Remove the three-terminal battery power MOSFET and bridge its source and
   drain pads so B+ remains connected to VDDH.
4. Bridge B+ to the SWD VDD pad. This connects B+, VDDH, and VDD and bypasses
   REG0.
5. For a CR2032 device that may be connected to USB, remove the onboard charger
   or disconnect its BAT pin from B+. A CR2032 must never be charged.

With W5 and the charger removed, USB no longer powers the main rail. The
battery must be installed while using USB; USB supplies only VBUS detection and
D+/D- data. Before the first USB connection, verify that VDD remains at the
battery voltage and never exceeds 3.6 V.

## Connections

- CR2032 positive: B+
- CR2032 negative: GND
- Button signal: nRF P0.20
- Button return: nRF P0.02 (firmware-controlled ground)
- Optional status LED: nRF P0.22, active high, with a series resistor

## Firmware behavior

- Button press uses GPIO SENSE LOW and the shared GPIOTE PORT event routed
  through PPI channel 15 to EGU3. No high-current GPIOTE IN channel is used.
- System OFF wake uses GPIO SENSE LOW on P0.20.
- System ON idle starts after 15 seconds.
- An existing BLE connection is retained in idle with low-power connection
  parameters.
- System OFF starts after four hours while BLE remains connected, or after
  1.5 hours without a BLE connection.
- Battery voltage is sampled directly from the SAADC VDD input with the
  internal 0.6 V reference and gain 1/6. No external divider is required.

## Measured result

On the modified board, the observed System ON idle floor was about 2.9 uA,
with brief connected BLE peaks up to about 120 uA, and System OFF was about
0.4-0.5 uA. Instantaneous readings vary because a multimeter averages short
radio-current pulses.

The firmware architecture is documented in
`docs/magic-button-nrf-architecture.md`.

Updating the UF2 bootloader from 0.6.0 to 0.9.2 did not change power
consumption. The material improvement came from bypassing VDDH/REG0 and
removing the leaking W5 path.
