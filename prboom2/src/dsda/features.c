//
// Copyright(C) 2022 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DSDA Features
//

#include "features.h"

static dsda_feature_t used_features;

void dsda_TrackFeature(dsda_feature_t feature) {
  used_features |= feature;
}

void dsda_ResetFeatures(void) {
  used_features = UF_NONE;
}

dsda_feature_t dsda_UsedFeatures(void) {
  return used_features;
}
