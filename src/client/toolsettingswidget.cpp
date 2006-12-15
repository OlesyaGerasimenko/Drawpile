/*
	DrawPile - a collaborative drawing program.

	Copyright (C) 2006 Calle Laakkonen

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software Foundation,
	Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QStackedWidget>

#include "toolsettingswidget.h"
#include "toolsettings.h"

namespace widgets {

ToolSettings::ToolSettings(QWidget *parent)
	: QDockWidget(parent)
{
	// Create a widget stack
	widgets_ = new QStackedWidget(this);
	setWidget(widgets_);

	// Create settings widget for brush
	brushsettings_ = new tools::BrushSettings("brush", tr("Brush settings"));
	widgets_->addWidget(brushsettings_->createUi(this));
	currenttool_ = brushsettings_;

	// Create settings widget for eraser
	erasersettings_ = new tools::BrushSettings("eraser", tr("Eraser settings"),true);
	widgets_->addWidget(erasersettings_->createUi(this));

	// Create a settings widget for color picker
	pickersettings_ = new tools::NoSettings("picker", tr("Color picker"));
	widgets_->addWidget(pickersettings_->createUi(this));

}

ToolSettings::~ToolSettings()
{
	delete brushsettings_;
	delete erasersettings_;
}

/**
 * Set which tool setting widget is visible
 * @param tool tool identifier
 */
void ToolSettings::setTool(tools::Type tool) {
	switch(tool) {
		case tools::BRUSH: currenttool_ = brushsettings_; break;
		case tools::ERASER: currenttool_ = erasersettings_; break;
		case tools::PICKER: currenttool_ = pickersettings_; break;
	}
	setWindowTitle(currenttool_->getTitle());
	widgets_->setCurrentWidget(currenttool_->getUi());
	currenttool_->setForeground(fgcolor_);
	currenttool_->setBackground(bgcolor_);
	emit sizeChanged(currenttool_->getSize());
}

void ToolSettings::setForeground(const QColor& color)
{
	fgcolor_ = color;
	currenttool_->setForeground(color);
}

void ToolSettings::setBackground(const QColor& color)
{
	bgcolor_ = color;
	currenttool_->setBackground(color);
}

/**
 * Get a brush with settings from the currently visible widget
 * @param foreground foreground color
 * @param background background color
 * @return brush
 */
drawingboard::Brush ToolSettings::getBrush() const
{
	return currenttool_->getBrush();
}

}

