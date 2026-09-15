// Terminal map for the Claudius Peters R8 ZML spout, and the translation between
// the eight board terminals and the machine-language Inputs/Outputs.
//
// The controller core never sees a terminal number. Everything hardware-shaped
// stops here, so a rewiring is a change to this file and nothing else.
#pragma once
#include <array>
#include <cstddef>
#include "sp01/cp/controller.hpp"

namespace sp01::cp {

// DI1..DI8 as printed on the board cover.
enum class Di : std::size_t {
    BagPresent = 0,      // DI1  PE-converter, air pressure in the holding rubber
    ProxStart,           // DI2  fixed switching vane, one pulse per revolution
    ProxDischarge,       // DI3  fixed cylinder, extended only when downstream runs
    AirActive,           // DI4  compressed air present
    BtnStart,            // DI5  spout switchboard
    BtnStop,             // DI6  spout switchboard
    BtnAck,              // DI7  fault acknowledge
    TurbineOverload,     // DI8  motor protection contact; leave low if unwired
};

// DO1..DO8 as printed on the board cover.
enum class Do : std::size_t {
    FeedMain = 0,        // DO1  shut-off gate, coarse position
    FeedDribble,         // DO2  shut-off gate, fine position
    BagHolder,           // DO3  bag clamping cylinder
    BagDischarge,        // DO4  bag discharge cylinder
    BlowOut,             // DO5  filling pipe blow-out impulse
    Aeration,            // DO6  labyrinth and filling vessel aeration
    Turbine,             // DO7  filling turbine contactor
    AlarmLamp,           // DO8  red lamp on the spout switchboard
};

constexpr bool di(const std::array<bool, 8>& image, Di channel) {
    return image[static_cast<std::size_t>(channel)];
}

// Board terminals -> machine language. weight_kg/weight_valid come from the TLB.
inline Inputs inputs_from_di(const std::array<bool, 8>& image,
                             float weight_kg, bool weight_valid) {
    Inputs in{};
    in.bag_present      = di(image, Di::BagPresent);
    in.prox_start       = di(image, Di::ProxStart);
    in.prox_discharge   = di(image, Di::ProxDischarge);
    in.air_active       = di(image, Di::AirActive);
    in.btn_start        = di(image, Di::BtnStart);
    in.btn_stop         = di(image, Di::BtnStop);
    in.btn_ack          = di(image, Di::BtnAck);
    in.turbine_overload = di(image, Di::TurbineOverload);
    in.weight_kg        = weight_kg;
    in.weight_valid     = weight_valid;
    return in;
}

// Machine language -> board terminals. Polarity inversion belongs to BoardIo.
inline std::array<bool, 8> di_image_to_do(const Outputs& out) {
    std::array<bool, 8> image{};
    image[static_cast<std::size_t>(Do::FeedMain)]     = out.feed_main;
    image[static_cast<std::size_t>(Do::FeedDribble)]  = out.feed_dribble;
    image[static_cast<std::size_t>(Do::BagHolder)]    = out.bag_holder;
    image[static_cast<std::size_t>(Do::BagDischarge)] = out.bag_discharge;
    image[static_cast<std::size_t>(Do::BlowOut)]      = out.blow_out;
    image[static_cast<std::size_t>(Do::Aeration)]     = out.aeration;
    image[static_cast<std::size_t>(Do::Turbine)]      = out.turbine;
    image[static_cast<std::size_t>(Do::AlarmLamp)]    = out.alarm_lamp;
    return image;
}

}  // namespace sp01::cp
