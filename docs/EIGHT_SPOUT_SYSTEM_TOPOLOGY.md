# SP01 View 13 — Eight-Spout System and Rotating/Stationary Topology

This view was added after Purple-Team design review. The single-spout views are necessary but insufficient for maintaining an 8-spout rotary packer as a complete machine.

## 1. System rule

```text
one spout = one independent controller
8 spouts = 8 peer controllers
no SP01 controller is master for another spout
```

Each controller keeps its own real-time loop local:

```text
DI -> controller FSM -> DO
TLB485 -> RS485 -> WeightSnapshot -> controller FSM
```

Loss of the supervisory network must not become loss of local cutoff/interlock authority.

## 2. Rotating versus stationary boundary

Current v0.1 architecture uses a stationary supervisory network and local rotating controllers:

```text
STATIONARY
operator laptop / HMI / optional collector
          |
          | supervisory network only
          v
Wi-Fi AP / service network
       )))   )))   )))

ROTATING
SP01  SP02 ... SP08
 |      |       |
local  local   local
FSM    FSM     FSM
TLB    TLB     TLB
I/O    I/O     I/O
```

The project does **not** currently assume that Ethernet/RS485/CAN data must pass through the mechanical slip ring.

A real slip-ring communication path may exist on the installed machine, but it must be documented from field evidence before becoming an architecture dependency.

## 3. What Purple Team correctly identified

A system-level communication view was missing. The important question is not 'which protocol sounds industrial?' but:

```text
what signals and power actually cross the rotating/stationary boundary?
what slip-ring channels already exist?
what electrical noise, impedance and grounding behavior are measured?
which links are required for control versus supervision?
```

Those answers determine whether a slip-ring data bus is useful.

## 4. Field topology audit

Before selecting any production inter-spout or central-HMI transport, record the as-built machine:

```text
slip-ring manufacturer/model if identifiable
number/type of rings
which rings carry AC/DC power
which rings carry legacy discrete signals
any existing shield/PE arrangement
available spare rings
existing Ethernet/fieldbus path if present
rotation speed
observed electrical noise/dropouts
stationary cabinet/network location
AP/antenna placement and RF coverage around one revolution
```

Do not infer a Gigabit/Ethernet-capable slip ring from connector appearance alone.

## 5. Supervisory HMI model

The central HMI/collector is a consumer of each spout's state, not a real-time master.

Recommended contract:

```text
SP01 -> state/weight/fault/cycle/diagnostics
SP02 -> state/weight/fault/cycle/diagnostics
...
SP08 -> state/weight/fault/cycle/diagnostics
             |
             v
central 8-spout overview / historian / evidence
```

The central view should support:

```text
8-spout health matrix
active state per spout
weight/fault summary
cycle timing comparison
network last-seen age
firmware/config identity
link to each local detailed HMI
```

A missing central HMI or lost packet must not directly command a fill cutoff.

## 6. Addressing and discovery

For commissioning, keep identity explicit:

```text
spout_id = SP01..SP08
firmware_sha
board identity
network identity
```

Preferred service practice is deterministic naming/addressing through documented DHCP reservation, static service configuration or another tested local mechanism. Do not make mDNS or cloud discovery a control prerequisite.

## 7. If a slip-ring data bus is later required

Choose the transport only after the field audit and measured noise/reliability test.

Candidate technologies may include:

```text
isolated RS485
CAN
industrial Ethernet / Ethernet through a rated rotary joint
wireless supervisory link
```

No candidate is automatically preferred in this document.

Selection criteria:

```text
required bandwidth
latency/jitter actually needed
error behavior during rotation
EMC/noise margin
isolation/grounding
connector/ring wear
serviceability
spare parts
ability to keep local control independent
```

If central communication is only supervisory, simpler and more fault-contained networking is preferred over introducing a new control dependency through the slip ring.

## 8. Power boundary

This view must be paired with the actual power topology. If controller power crosses the slip ring, voltage drop, transient behavior, brownout and grounding must be characterized independently from the data network.

G2T/G8 evidence should correlate reset/brownout events with rotor position and representative machine switching where practical.

## 9. Gate mapping

```text
G2/G2T  local board/network/power evidence
G6      supervisory network loss while local controller continues correctly
G7      freeze the reviewed 8-spout topology drawing and identity scheme
G8      verify real rotating/stationary communication behavior in shadow
G9      central HMI remains supervisory; no new remote output authority without separate review
```

This view is the canonical system-level topology companion to the single-spout engineering views.