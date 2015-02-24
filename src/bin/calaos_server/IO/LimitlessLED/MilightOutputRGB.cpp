/******************************************************************************
 **  Copyright (c) 2006-2014, Calaos. All Rights Reserved.
 **
 **  This file is part of Calaos.
 **
 **  Calaos is free software; you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation; either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Calaos is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Foobar; if not, write to the Free Software
 **  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 **
 ******************************************************************************/
#include "MilightOutputRGB.h"
#include "IOFactory.h"

using namespace Calaos;

REGISTER_OUTPUT(MilightOutputRGB)

MilightOutputRGB::MilightOutputRGB(Params &p):
    OutputLightRGB(p)
{
    host = get_param("host");
    if (get_params().Exists("port"))
        Utils::from_string(get_param("port"), port);
    if (get_params().Exists("zone"))
        Utils::from_string(get_param("zone"), zone);

    milight = new Milight(host, port);
}

MilightOutputRGB::~MilightOutputRGB()
{
    DELETE_NULL(milight);
}

void MilightOutputRGB::setColorReal(const ColorValue &c, bool s)
{
    if (!s)
    {
        milight->sendOffCommand(zone);
    }
    else
    {
        ushort micolor = milight->calcMilightColor(c);
        milight->sendColorCommand(zone, micolor);
        EcoreTimer::singleShot(0.2, [=]()
        {
            double v = (double(c.getHSLLightness()) * 18.) / 255.;
            milight->sendBrightnessCommand(zone, int(v) + 1);
        });
    }
}
