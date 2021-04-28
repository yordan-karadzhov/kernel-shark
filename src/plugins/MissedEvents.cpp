// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2018 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

/**
 *  @file    MissedEvents.cpp
 *  @brief   Plugin for visualization of events, missed due to overflow of the ring buffer.
 */

// C++
#include<iostream>

// KernelShark
#include "libkshark.h"
#include "plugins/missed_events.h"
#include "KsPlotTools.hpp"
#include "KsPlugins.hpp"

using namespace KsPlot;

/**
 * This class represents the graphical element of the KernelShark marker for
 * Missed events.
 */
class MissedEventsMark : public PlotObject {
public:
	/** No default constructor. */
	MissedEventsMark() = delete;

	/**
	 * @brief Create and position a Missed events marker.
	 *
	 * @param p: Base point of the marker.
	 * @param h: vertical size (height) of the marker.
	 */
	MissedEventsMark(const Point &p, int h)
	: _base(p.x(), p.y()), _height(h) {}

private:
	/** Base point of the Mark's line. */
	Point	_base;

	/** The vertical size (height) of the Mark. */
	int	_height;

	void _draw(const Color &col, float size = 1.) const override;
};

void MissedEventsMark::_draw(const Color &col, float size) const
{
	Point p(_base.x(), _base.y() - _height);
	drawLine(_base, p, col, size);

	Rectangle rec;
	rec.setPoint(0, p.x(), p.y());
	rec.setPoint(1, p.x() - _height / 4, p.y());
	rec.setPoint(2, p.x() - _height / 4, p.y() + _height / 4);
	rec.setPoint(3, p.x(), p.y() + _height / 4);
	rec._color = col;
	rec.draw();
}

static PlotObject *makeShape(std::vector<const Graph *> graph,
			     std::vector<int> bin,
			     std::vector<kshark_data_field_int64 *>,
			     Color col, float size)
{
	MissedEventsMark *mark = new MissedEventsMark(graph[0]->bin(bin[0])._base,
						      graph[0]->height());
	mark->_size = size;
	mark->_color = col;

	return mark;
}

//! @cond Doxygen_Suppress

#define PLUGIN_MAX_ENTRIES		10000

//! @endcond

/**
 * @brief Plugin's draw function.
 *
 * @param argv_c: A C pointer to be converted to KsCppArgV (C++ struct).
 * @param sd: Data stream identifier.
 * @param val: Process or CPU Id value.
 * @param draw_action: Draw action identifier.
 */
__hidden void draw_missed_events(kshark_cpp_argv *argv_c,
			int sd, int val, int draw_action)
{
	KsCppArgV *argvCpp = KS_ARGV_TO_CPP(argv_c);
	IsApplicableFunc checkEntry;

	/*
	 * Plotting the "Missed events" makes sense only in the case of a deep
	 * zoom. Here we set a threshold based on the total number of entries
	 * being visualized by the model.
	 * Don't be afraid to play with different values for this threshold.
	 */
	if (argvCpp->_histo->tot_count > PLUGIN_MAX_ENTRIES)
		return;

	if (!(draw_action & KSHARK_CPU_DRAW) &&
	    !(draw_action & KSHARK_TASK_DRAW))
		return;

	if (draw_action & KSHARK_CPU_DRAW)
		checkEntry = [=] (kshark_data_container *, ssize_t bin) {
			return !!ksmodel_get_cpu_missed_events(argvCpp->_histo,
							       bin, sd, val,
							       nullptr,
							       nullptr);
		};

	else if (draw_action & KSHARK_TASK_DRAW)
		checkEntry = [=] (kshark_data_container *, ssize_t bin) {
			return !!ksmodel_get_task_missed_events(argvCpp->_histo,
								bin, sd, val,
								nullptr,
								nullptr);
		};

	eventPlot(argvCpp,
		  checkEntry,
		  makeShape,
		  {0, 0, 255}, // Blue
		  -1);         // Default size
}
