/*
 * Copyright 2015-2017 Clément Vuchener
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

#include "MacroText.h"

#include <sstream>
#include <map>
#include <regex>

#include <misc/Log.h>
#include <misc/UsageStrings.h>

using HIDPP::Macro;

std::string macroToText (Macro::const_iterator begin, Macro::const_iterator end)
{
	unsigned int next_label = 0;
	std::map<const Macro::Item *, std::string> labels;

	for (auto it = begin; it != end; ++it) {
		const Macro::Item &item = *it;
		if (item.isJump ()) {
			Macro::const_iterator dest = item.jumpDestination ();
			if (labels.find (&(*dest)) != labels.end ())
				continue;
			std::stringstream ss;
			ss << "label" << next_label++;
			labels.insert ({&(*dest), ss.str ()});
		}
	}

	std::stringstream ss;

	for (auto it = begin; it != end; ++it) {
		const Macro::Item &item = *it;
		auto label = labels.find (&item);
		if (label != labels.end ()) {
			ss << label->second << ":" << std::endl;
		}

		ss << Macro::Item::InstructionStrings.at (item.instruction ());

		switch (item.instruction ()) {
		case Macro::Item::KeyPress:
		case Macro::Item::KeyRelease:
			ss << " " << keyString (item.keyCode ());
			break;

		case Macro::Item::ModifiersPress:
		case Macro::Item::ModifiersRelease:
			ss << " " << modifierString (item.modifiers ());
			break;

		case Macro::Item::ModifiersKeyPress:
		case Macro::Item::ModifiersKeyRelease:
			ss << " " << modifierString (item.modifiers ())
			   << " " << keyString (item.keyCode ());
			break;

		case Macro::Item::MouseWheel:
		case Macro::Item::MouseHWheel:
			ss << " " << item.wheel ();
			break;

		case Macro::Item::MouseButtonPress:
		case Macro::Item::MouseButtonRelease:
			ss << " " << buttonString (item.buttons ());
			break;

		case Macro::Item::ConsumerControl:
		case Macro::Item::ConsumerControlPress:
		case Macro::Item::ConsumerControlRelease:
			ss << " " << consumerControlString (item.consumerControl ());
			break;

		case Macro::Item::Delay:
		case Macro::Item::ShortDelay:
			ss << " " << item.delay ();
			break;

		case Macro::Item::Jump:
		case Macro::Item::JumpIfPressed:
			ss << " " << labels[&(*item.jumpDestination ())];
			break;

		case Macro::Item::MousePointer:
			ss << " " << item.mouseX () << " " << item.mouseY ();
			break;

		case Macro::Item::JumpIfReleased:
			ss << " " << item.delay ()
			   << " " << labels[&(*item.jumpDestination ())];
			break;

		default:
			break;
		}
		ss << ";" << std::endl;
	}

	return ss.str ();
}

Macro textToMacro (const std::string &text)
{
	static const std::regex InstructionRegex ("(?:\\s*(\\w+):)?\\s*(\\w+)(?:\\s+([^;]+))?\\s*(?:;|$)");
	static const std::regex ParamRegex ("\\S+");

	std::map<std::string, Macro::iterator> labels;
	std::vector<std::pair<Macro::Item *, std::string>> jumps;

	Macro macro;

	std::string::const_iterator current = text.begin ();
	std::smatch results;
	while (std::regex_search (current, text.end (),
				  results, InstructionRegex,
				  std::regex_constants::match_continuous)) {
		std::string label = results.str (1);
		std::string instruction = results.str (2);
		std::vector<std::string> params;

		std::smatch param_res;
		std::string::const_iterator current_param = results[3].first;
		while (std::regex_search (current_param, results[3].second, param_res, ParamRegex)) {
			params.push_back (param_res.str (0));
			current_param = param_res[0].second;
		}

		Macro::Item *item = nullptr;
		for (const auto &p: Macro::Item::InstructionStrings) {
			if (instruction == p.second) {
				macro.emplace_back (p.first);
				item = &macro.back ();
				break;
			}
		}
		if (!item) {
			Log::error () << "Unknown instruction " << instruction << std::endl;
			return Macro ();
		}
		switch (item->instruction ()) {
		case Macro::Item::KeyPress:
		case Macro::Item::KeyRelease: {
			unsigned int code = keyUsageCode (params[0]);
			if (code > 255)
				Log::warning () << "Key code " << code << " is too big." << std::endl;
			item->setKeyCode (static_cast<uint8_t> (code));
			break;
		}
		case Macro::Item::ModifiersPress:
		case Macro::Item::ModifiersRelease: {
			uint8_t mask = modifierMask (params[0]);
			item->setModifiers (mask);
			break;
		}
		case Macro::Item::ModifiersKeyPress:
		case Macro::Item::ModifiersKeyRelease: {
			uint8_t mask = modifierMask (params[0]);
			item->setModifiers (mask);
			unsigned int code = keyUsageCode (params[1]);
			if (code > 255)
				Log::warning () << "Key code " << code << " is too big." << std::endl;
			item->setKeyCode (static_cast<uint8_t> (code));
			break;
		}
		case Macro::Item::MouseWheel:
		case Macro::Item::MouseHWheel: {
			int wheel = std::stoi (params[0]);
			item->setWheel (wheel);
			break;
		}
		case Macro::Item::MouseButtonPress:
		case Macro::Item::MouseButtonRelease: {
			unsigned int mask = buttonMask (params[0]);
			if (mask > 65535)
				Log::warning () << "Button number too big." << std::endl;
			item->setButtons (mask);
			break;
		}
		case Macro::Item::ConsumerControl:
		case Macro::Item::ConsumerControlPress:
		case Macro::Item::ConsumerControlRelease: {
			unsigned int code = consumerControlCode (params[0]);
			item->setConsumerControl (code);
			break;
		}
		case Macro::Item::Delay:
		case Macro::Item::ShortDelay: {
			unsigned int delay = std::stoul (params[0]);
			item->setDelay (delay);
			break;
		}
		case Macro::Item::Jump:
		case Macro::Item::JumpIfPressed:
			jumps.emplace_back (item, params[0]);
			break;
		case Macro::Item::MousePointer: {
			int x = std::stoi (params[0]);
			int y = std::stoi (params[1]);
			item->setMouseX (x);
			item->setMouseY (y);
			break;
		}
		case Macro::Item::JumpIfReleased: {
			unsigned int delay = std::stoul (params[0]);
			item->setDelay (delay);
			jumps.emplace_back (item, params[1]);
			break;
		}
		default:
			break;
		}

		if (!label.empty ()) {
			labels.emplace (label, std::prev (macro.end ()));
		}

		current = results[0].second;
	}

	for (auto pair: jumps) {
		Macro::Item *item = pair.first;
		const std::string &label = pair.second;
		auto it = labels.find (label);
		if (it == labels.end ()) {
			Log::error () << "Unknown label " << label << std::endl;
			return Macro ();
		}
		item->setJumpDestination (it->second);
	}

	return macro;
}
