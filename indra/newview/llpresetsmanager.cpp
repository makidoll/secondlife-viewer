/**
 * @file llpresetsmanager.cpp
 * @brief Implementation for the LLPresetsManager class.
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include <boost/assign/list_of.hpp>

#include "llpresetsmanager.h"

#include "lldiriterator.h"
#include "llfloater.h"
#include "llsdserialize.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"

LLPresetsManager::LLPresetsManager()
{
}

LLPresetsManager::~LLPresetsManager()
{
}

//std::string LLPresetsManager::getUserDir(const std::string& subdirectory)
std::string LLPresetsManager::getPresetsDir(const std::string& subdirectory)
{
	std::string presets_path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_DIR);
	std::string full_path;

	if (!gDirUtilp->fileExists(presets_path))
	{
		LLFile::mkdir(presets_path);
	}

	full_path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_DIR, subdirectory);
	if (!gDirUtilp->fileExists(full_path))
	{
		LLFile::mkdir(full_path);
	}

	return full_path;
}

void LLPresetsManager::loadPresetNamesFromDir(const std::string& dir, preset_name_list_t& presets)
{
	LL_INFOS("AppInit") << "Loading presets from " << dir << LL_ENDL;

	mPresetNames.clear();

	LLDirIterator dir_iter(dir, "*.xml");
	bool found = true;
	while (found)
	{
		std::string file;
		found = dir_iter.next(file);

		if (found)
		{
			std::string path = gDirUtilp->add(dir, file);
			std::string name(gDirUtilp->getBaseFileName(LLURI::unescape(path), /*strip_exten = */ true));
			if ("Default" != name)
			{
				mPresetNames.push_back(name);
			}
			else
			{
				mPresetNames.insert(mPresetNames.begin(), name);
			}
		}
	}

	presets = mPresetNames;
}

void LLPresetsManager::savePreset(const std::string& subdirectory, const std::string& name)
{
	llassert(!name.empty());

	std::vector<std::string> name_list;
	// This ugliness is the current list of all the control variables in the graphics and hardware
	// preferences floaters or the settings for camera views.
	// Additions or subtractions to the control variables in the floaters must also be reflected here.
	if(PRESETS_GRAPHIC == subdirectory)
	{
		name_list = boost::assign::list_of
			("RenderQualityPerformance")
			("RenderFarClip")
			("RenderMaxPartCount")
			("RenderGlowResolutionPow")
			("RenderTerrainDetail")
			("RenderAvatarLODFactor")
			("RenderAvatarMaxVisible")
			("RenderUseImpostors")
			("RenderTerrainLODFactor")
			("RenderTreeLODFactor")
			("RenderVolumeLODFactor")
			("RenderFlexTimeFactor")
			("RenderTransparentWater")
			("RenderObjectBump")
			("RenderLocalLights")
			("VertexShaderEnable")
			("RenderAvatarVP")
			("RenderAvatarCloth")
			("RenderReflectionDetail")
			("WindLightUseAtmosShaders")
			("WLSkyDetail")
			("RenderDeferred")
			("RenderDeferredSSAO")
			("RenderDepthOfField")
			("RenderShadowDetail")

			("RenderAnisotropic")
			("RenderFSAASamples")
			("RenderGamma")
			("RenderVBOEnable")
			("RenderCompressTextures")
			("TextureMemory")
			("RenderFogRatio");
		}

	if(PRESETS_CAMERA == subdirectory)
	{
		name_list = boost::assign::list_of
			("Placeholder");
	}

	// make an empty llsd
	LLSD paramsData(LLSD::emptyMap());

	for (std::vector<std::string>::iterator it = name_list.begin(); it != name_list.end(); ++it)
	{
		std::string ctrl_name = *it;
		LLControlVariable* ctrl = gSavedSettings.getControl(ctrl_name).get();
		std::string comment = ctrl->getComment();
		std::string type = gSavedSettings.typeEnumToString(ctrl->type());
		LLSD value = ctrl->getValue();

		paramsData[ctrl_name]["Comment"] =  comment;
		paramsData[ctrl_name]["Persist"] = 1;
		paramsData[ctrl_name]["Type"] = type;
		paramsData[ctrl_name]["Value"] = value;
	}

	std::string pathName(getPresetsDir(subdirectory) + "\\" + LLURI::escape(name) + ".xml");

	// write to file
	llofstream presetsXML(pathName);
	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
	formatter->format(paramsData, presetsXML, LLSDFormatter::OPTIONS_PRETTY);
	presetsXML.close();

	// signal interested parties
	mPresetListChangeSignal();
}

void LLPresetsManager::setPresetNamesInComboBox(const std::string& subdirectory, LLComboBox* combo)
{
	combo->clearRows();

	std::string presets_dir = getPresetsDir(subdirectory);

	if (!presets_dir.empty())
	{
		std::list<std::string> preset_names;
		loadPresetNamesFromDir(presets_dir, preset_names);

		combo->setLabel(LLTrans::getString("preset_combo_label"));

		for (std::list<std::string>::const_iterator it = preset_names.begin(); it != preset_names.end(); ++it)
		{
			const std::string& name = *it;
			combo->add(name, LLSD().with(0, name));
		}
	}
	else
	{
		LL_WARNS() << "Could not obtain graphic presets path" << LL_ENDL;
	}
}

void LLPresetsManager::loadPreset(const std::string& name)
{
	std::string full_path(getPresetsDir(PRESETS_GRAPHIC) + "\\" + LLURI::escape(name) + ".xml");

	gSavedSettings.loadFromFile(full_path, false, true);
}

bool LLPresetsManager::deletePreset(const std::string& name)
{
	// remove from param list
	preset_name_list_t::iterator it = find(mPresetNames.begin(), mPresetNames.end(), name);
	if (it == mPresetNames.end())
	{
		LL_WARNS("Presets") << "No preset named " << name << LL_ENDL;
		return false;
	}

	if (gDirUtilp->deleteFilesInDir(getPresetsDir(PRESETS_GRAPHIC), LLURI::escape(name) + ".xml") < 1)
	{
		LL_WARNS("Presets") << "Error removing preset " << name << " from disk" << LL_ENDL;
		return false;
	}
	else
	{
		mPresetNames.erase(it);
	}

	// signal interested parties
	mPresetListChangeSignal();

	return true;
}

boost::signals2::connection LLPresetsManager::setPresetListChangeCallback(const preset_list_signal_t::slot_type& cb)
{
	return mPresetListChangeSignal.connect(cb);
}
