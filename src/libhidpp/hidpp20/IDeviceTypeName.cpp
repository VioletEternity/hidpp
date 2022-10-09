/*
 * Copyright 2022 VioletEternity
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <hidpp20/IDeviceTypeName.h>

#include <hidpp20/Device.h>

#include <misc/Endian.h>

using namespace HIDPP20;

constexpr uint16_t IDeviceTypeName::ID;

IDeviceTypeName::IDeviceTypeName (Device *dev):
	FeatureInterface (dev, ID, "DeviceTypeName")
{
}

std::string IDeviceTypeName::getDeviceName()
{
    std::vector<uint8_t> results;
	results = call (GetDeviceNameCount);
	size_t count = static_cast<size_t> (results[0]);

    std::string name;
    while (name.size () < count) {
        std::vector<uint8_t> params (1);
        params[0] = static_cast<uint8_t> (name.size ());
        results = call (GetDeviceName, params);
        name.append(results.begin (), results.begin() + std::min(results.size (), count - name.size ()));
    }
    return name;
}

IDeviceTypeName::DeviceType IDeviceTypeName::getDeviceType()
{
    std::vector<uint8_t> results;
    results = call (GetDeviceType);
    return static_cast<DeviceType> (results[0]);
}