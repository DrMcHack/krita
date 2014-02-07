/* This file is part of the KDE project
 * Copyright (c) 2009 Cyrille Berger <cberger@cberger.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "kis_pressure_rotation_option.h"
#include "kis_pressure_opacity_option.h"
#include "sensors/kis_dynamic_sensor_list.h"
#include "sensors/kis_dynamic_sensor_drawing_angle.h"
#include "sensors/kis_dynamic_sensor_fuzzy.h"
#include <klocale.h>
#include <kis_painter.h>
#include <kis_paintop.h>
#include <KoColor.h>

KisPressureRotationOption::KisPressureRotationOption()
        : KisCurveOption(i18n("Rotation"), "Rotation", KisPaintOpOption::brushCategory(), false),
          m_defaultAngle(0.0),
          m_canvasAxisXMirrored(false),
          m_canvasAxisYMirrored(false)
{

     setMinimumLabel(i18n("0°"));
     setMaximumLabel(i18n("360°"));
     m_fuzzyflag = true;

}

double KisPressureRotationOption::apply(const KisPaintInformation & info) const
{
    if (!isChecked()) return m_defaultAngle;

    bool dependsOnViewportTransformations = sensor()->dependsOnCanvasRotation();

    qreal baseAngle = dependsOnViewportTransformations ? m_defaultAngle : 0;
    qreal rotationCoeff =
        dependsOnViewportTransformations ||
        m_canvasAxisXMirrored == m_canvasAxisYMirrored ?
        1.0 - computeValue(info) :
        0.5 + computeValue(info);

    /* Special Case for Fuzzy Sensor to provide for Positive and Negative Rotation */
    KisDynamicSensor *sensor = this->sensor();
    KisDynamicSensorList *sensorList;
    if (sensor->id() == FuzzyId.id() ||
        ((sensorList = dynamic_cast<KisDynamicSensorList*>(sensor)) &&
         (sensor = sensorList->getSensor(FuzzyId.id()))))
    {

            KisDynamicSensorFuzzy *s =
                dynamic_cast<KisDynamicSensorFuzzy*>(sensor);
            Q_ASSERT(s);
            if(s->rotationModeEnabled())
            {
                if(m_fuzzyflag == true)
                {
                    m_fuzzyflag = false;
                    return fmod(rotationCoeff * 2.0 * M_PI + baseAngle, 2.0 * M_PI);
                }
                else
                {
                    m_fuzzyflag = true;
                    return ((2.0*M_PI)-fmod(rotationCoeff * 2.0 * M_PI + baseAngle, -2.0 * M_PI));

                }
            }
            else
            {
                return fmod(rotationCoeff * 2.0 * M_PI + baseAngle, 2.0 * M_PI);
            }
    }
    else //If Fuzzy is not selected at all
    {
        return fmod(rotationCoeff * 2.0 * M_PI + baseAngle, 2.0 * M_PI);

    }


 }

void KisPressureRotationOption::readOptionSetting(const KisPropertiesConfiguration* setting)
{
    m_defaultAngle = setting->getDouble("runtimeCanvasRotation", 0.0) * M_PI / 180.0;
    KisCurveOption::readOptionSetting(setting);

    m_canvasAxisXMirrored = setting->getBool("runtimeCanvasMirroredX", false);
    m_canvasAxisYMirrored = setting->getBool("runtimeCanvasMirroredY", false);
}

void KisPressureRotationOption::applyFanCornersInfo(KisPaintOp *op)
{
    KisDynamicSensor *sensor = this->sensor();
    KisDynamicSensorList *sensorList;

    /**
     * A special case for the Drawing Angle sensor, because it
     * changes the behavior of KisPaintOp::paintLine()
     */

    if (sensor->id() == DrawingAngleId.id() ||
        ((sensorList = dynamic_cast<KisDynamicSensorList*>(sensor)) &&
         (sensor = sensorList->getSensor(DrawingAngleId.id())))) {

        KisDynamicSensorDrawingAngle *s =
            dynamic_cast<KisDynamicSensorDrawingAngle*>(sensor);
        Q_ASSERT(s);

        op->setFanCornersInfo(s->fanCornersEnabled(), qreal(s->fanCornersStep()) * M_PI / 180.0);
    }
}
