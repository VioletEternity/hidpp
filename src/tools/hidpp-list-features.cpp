/*
 * Copyright 2015 Clément Vuchener
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

#include <cstdio>

#include <hidpp/Device.h>
#include <hidpp10/Device.h>
#include <hidpp10/Error.h>
#include <hidpp10/IIndividualFeatures.h>
#include <hidpp20/Device.h>
#include <hidpp20/IFeatureSet.h>
#include <misc/Log.h>

#include "common/common.h"
#include "common/Option.h"
#include "common/CommonOptions.h"

void testRegister (HIDPP10::Device *dev, std::size_t register_size, uint8_t address, bool test_write = false)
{
	std::vector<uint8_t> values (register_size);
	bool readable = false;
	try {
		dev->getRegister (address, nullptr, values);
		readable = true;
		printf ("Register 0x%02hhx read  %2lu:", address, register_size);
		for (uint8_t value: values)
			printf (" %02hhx", value);
		printf ("\n");
	}
	catch (HIDPP10::Error e) {
		if (e.errorCode () != HIDPP10::Error::InvalidSubID &&
		    e.errorCode () != HIDPP10::Error::InvalidAddress) {
			printf ("Register 0x%02hhx read  %2lu: %s (0x%02hhx)\n",
				address, register_size, e.what (), e.errorCode ());
		}
	}
	if (test_write) {
		try {
			std::vector<uint8_t> results;
			if (readable) {
				dev->setRegister (address, values, &results);
			}
			else {
				std::vector<uint8_t> params (register_size);
				dev->setRegister (address, params, &results);
			}
			printf ("Register 0x%02hhx write %2lu:", address, register_size);
			for (uint8_t value: results)
				printf (" %02hhx", value);
			printf ("\n");
		}
		catch (HIDPP10::Error e) {
			if (e.errorCode () != HIDPP10::Error::InvalidSubID &&
			    e.errorCode () != HIDPP10::Error::InvalidAddress) {
				printf ("Register 0x%02hhx write %2lu: %s (0x%02hhx)\n",
					address, register_size, e.what (), e.errorCode ());
			}
		}
		catch (std::system_error e) {
			/* G5 does not support long writes and throw EPIPE */
			if (e.code ().value () != EPIPE) {
				throw e;
			}
		}
	}
}

int main (int argc, char *argv[])
{
	static const char *args = "/dev/hidrawX";
	HIDPP::DeviceIndex device_index = HIDPP::DefaultDevice;
	bool do_write_tests = false;

	std::vector<Option> options = {
		DeviceIndexOption (device_index),
		VerboseOption (),
		Option ('w', "write",
			Option::NoArgument, "",
			"Also do write tests with HID++ 1.0 devices.",
			[&do_write_tests] (const char *optarg) -> bool {
				do_write_tests = true;
				return true;
			})
	};
	Option help = HelpOption (argv[0], args, &options);
	options.push_back (help);

	int first_arg;
	if (!Option::processOptions (argc, argv, options, first_arg))
		return EXIT_FAILURE;

	if (argc-first_arg != 1) {
		fprintf (stderr, "%s", getUsage (argv[0], args, &options).c_str ());
		return EXIT_FAILURE;
	}

	const char *path = argv[first_arg];

	/*
	 * Check protocol version
	 */
	unsigned int major, minor;
	try {
		HIDPP::Device dev (path, device_index);
		dev.getProtocolVersion (major, minor);
		printf ("%s (%04hx:%04hx) is a HID++ %d.%d device\n",
			dev.name ().c_str (),
			dev.vendorID (), dev.productID (),
			major, minor);

	}
	catch (HIDPP::Device::NoHIDPPReportException e) {
		printf ("%s is not a HID++ device\n", path);
		return EXIT_FAILURE;
	}
	catch (std::system_error e) {
		fprintf (stderr, "Failed to open %s: %s\n", path, e.what ());
		return EXIT_FAILURE;
	}
	/*
	 * HID++ 1.0
	 */
	if (major == 1 && minor == 0) {
		HIDPP10::Device dev (path, device_index);

		for (unsigned int address = 0; address < 256; ++address) {
			testRegister (&dev, HIDPP::ShortParamLength, static_cast<uint8_t> (address), do_write_tests);
			testRegister (&dev, HIDPP::LongParamLength, static_cast<uint8_t> (address), do_write_tests);
		}
		if (do_write_tests) {
			try {
				HIDPP10::IIndividualFeatures iif (&dev);
				unsigned int old_flags = iif.flags ();
				iif.setFlags (0x00FFFFFF);
				unsigned int flags = iif.flags ();
				iif.setFlags (old_flags);
				printf ("Individual features: %06x\n", flags);
				if (flags & HIDPP10::IIndividualFeatures::SpecialButtonFunction) {
					printf (" - Special Button Function\n");
				}
				if (flags & HIDPP10::IIndividualFeatures::EnhancedKeyUsage) {
					printf (" - Enhanced Key Usage\n");
				}
				if (flags & HIDPP10::IIndividualFeatures::FastForwardRewind) {
					printf (" - Fast Forward/Rewind\n");
				}
				if (flags & HIDPP10::IIndividualFeatures::ScrollingAcceleration) {
					printf (" - Scrolling Acceleration\n");
				}
				if (flags & HIDPP10::IIndividualFeatures::ButtonsControlResolution) {
					printf (" - Buttons Control Resolution\n");
				}
				if (flags & HIDPP10::IIndividualFeatures::InhibitLockKeySound) {
					printf (" - Inhibit Lock KeyS ound\n");
				}
				if (flags & HIDPP10::IIndividualFeatures::MXAir3DEngine) {
					printf (" - 3D Engine\n");
				}
				if (flags & HIDPP10::IIndividualFeatures::LEDControl) {
					printf (" - LED Control\n");
				}
			}
			catch (HIDPP10::Error e) {
				printf ("Individual features: %s\n", e.what ());
			}
		}
	}
	/*
	 * HID++ 2.0 and later
	 */
	else if (major >= 2) {
		HIDPP20::Device dev (path, device_index);
		HIDPP20::IFeatureSet ifeatureset (&dev);

		unsigned int feature_count = ifeatureset.getCount ();
		for (unsigned int i = 1; i <= feature_count; ++i) {
			uint8_t feature_index = i;
			uint16_t feature_id;
			bool obsolete, hidden, internal;

			feature_id = ifeatureset.getFeatureID (feature_index, &obsolete, &hidden, &internal);
			printf ("Feature 0x%02hhx: [0x%04hx]", feature_index, feature_id);
			if (obsolete)
				printf (" obsolete");
			if (hidden)
				printf (" hidden");
			if (internal)
				printf (" internal");
			printf ("\n");
		}
	}
	else {
		fprintf (stderr, "Unsupported HID++ protocol version.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

