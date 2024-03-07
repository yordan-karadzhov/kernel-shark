// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <y.karadz@gmail.com>
 */

/**
 *  @file    KsPlotTools.cpp
 *  @brief   KernelShark Plot tools.
 */

// C
#include <cstring>
#include <math.h>

// C++
#include <algorithm>
#include <vector>

// KernelShark
#include "KsPlotTools.hpp"

namespace KsPlot
{

float Color::_frequency = .75;

/**
 * @brief Create a default color (black).
 */
Color::Color()
{
	_col_c.red = _col_c.green = _col_c.blue = 0;
}

/**
 * @brief Constructs a RGB color object.
 *
 * @param r: The red component of the color.
 * @param g: The green component of the color.
 * @param b: The blue component of the color
 */
Color::Color(uint8_t r, uint8_t g, uint8_t b)
{
	set(r, g, b);
}

/**
 * @brief Constructs a RGB color object.
 *
 * @param rgb: RGB value.
 */
Color::Color(int rgb)
{
	set(rgb);
}

/**
 * @brief Sets the color.
 *
 * @param r: The red component of the color.
 * @param g: The green component of the color.
 * @param b: The blue component of the color
 */
void Color::set(uint8_t r, uint8_t g, uint8_t b)
{
	_col_c.red = r;
	_col_c.green = g;
	_col_c.blue = b;
}

/**
 * @brief Sets the color.
 *
 * @param rgb: RGB value.
 */
void Color::set(int rgb)
{
	int r = rgb & 0xFF;
	int g = (rgb >> 8) & 0xFF;
	int b = (rgb >> 16) & 0xFF;

	set(r, g, b);
}

/**
 * @brief The color is selected from the Rainbow palette.
 *
 * @param n: index of the color inside the Rainbow palette.
 */
void Color::setRainbowColor(int n)
{
	int r = sin(_frequency * n + 0) * 127 + 128;
	int g = sin(_frequency * n + 2) * 127 + 128;
	int b = sin(_frequency * n + 4) * 127 + 128;

	set(r, g, b);
}

/** Alpha blending with white background. */
void Color::blend(float alpha)
{
	if (alpha < 0 || alpha > 1.)
		return;

	auto lamBld = [alpha](int val) {
		return  val * alpha + (1 - alpha) * 255;
	};

	int r = lamBld(this->r());
	int g = lamBld(this->g());
	int b = lamBld(this->b());

	set(r, g, b);
}

/**
 * @brief Create a Hash table of Rainbow colors. The sorted Pid values are
 *	  mapped to the palette of Rainbow colors.
 *
 * @returns ColorTable instance.
 */
ColorTable taskColorTable()
{
	struct kshark_context *kshark_ctx(nullptr);
	int nTasks(0), pid, *pids, i(0), *streamIds;
	std::vector<int> tempPids;
	ColorTable colors;

	if (!kshark_instance(&kshark_ctx))
		return colors;

	streamIds = kshark_all_streams(kshark_ctx);
	for (int j = 0; j < kshark_ctx->n_streams; ++j) {
		nTasks = kshark_get_task_pids(kshark_ctx, streamIds[j], &pids);
		tempPids.insert(tempPids.end(), pids, pids + nTasks);
		free(pids);
	}

	free(streamIds);

	if (!tempPids.size())
		return colors;

	std::sort(tempPids.begin(), tempPids.end());

	/* Erase duplicates. */
	tempPids.erase(unique(tempPids.begin(), tempPids.end()),
		       tempPids.end());

	/* Erase negative (error codes). */
	auto isNegative = [](int i) {return i < 0;};
	auto it = remove_if(tempPids.begin(), tempPids.end(), isNegative);
	tempPids.erase(it, tempPids.end());

	nTasks = tempPids.size();
	if (tempPids[i] == 0) {
		/* Pid <= 0 will be plotted in black. */
		colors[i++] = {};
	}

	for (; i < nTasks; ++i) {
		pid = tempPids[i];
		colors[pid].setRainbowColor(i);
	}

	return colors;
}

/**
 * @brief Create a Hash table of Rainbow colors. The CPU Ids are
 *	  mapped to the palette of Rainbow colors.
 *
 * @returns ColorTable instance.
 */
ColorTable CPUColorTable()
{
	kshark_context *kshark_ctx(nullptr);
	int nCPUs, nCPUMax(0), *streamIds;
	kshark_data_stream *stream;
	ColorTable colors;

	if (!kshark_instance(&kshark_ctx))
		return colors;

	streamIds = kshark_all_streams(kshark_ctx);
	for (int i = 0; i < kshark_ctx->n_streams; ++i) {
		stream = kshark_get_data_stream(kshark_ctx, streamIds[i]);
		nCPUs =  stream->n_cpus;
		if (nCPUMax < nCPUs)
			nCPUMax = nCPUs;
	}

	free(streamIds);

	for (int i = 0; i < nCPUMax; ++i)
		colors[i].setRainbowColor(i + 8);

	return colors;
}

/**
 * @brief Create a Hash table of Rainbow colors. The Steam Ids are
 *	  mapped to the palette of Rainbow colors.
 *
 * @returns ColorTable instance.
 */
ColorTable streamColorTable()
{
	kshark_context *kshark_ctx(nullptr);
	ColorTable colors;
	Color color;
	float alpha(.35);
	int *streamIds;

	if (!kshark_instance(&kshark_ctx))
		return colors;

	streamIds = kshark_all_streams(kshark_ctx);
	for (int i = 0; i < kshark_ctx->n_streams; ++i) {
		/*
		 * Starting from index 6 provides better functioning of the
		 * color scheme slider.
		 */
		color.setRainbowColor(i + 6);
		color.blend(alpha);
		colors[streamIds[i]] = color;
	}

	free(streamIds);

	return colors;
}

/**
 * @brief Search the Hash table of Rainbow colors for a particular key (Id).
 *
 * @param colors: Input location for the ColorTable instance.
 * @param id: The Id to search for.
 *
 * @returns The Rainbow color of the key "id". If "id" does not exist, the
 *	    returned color is Black.
 */
Color getColor(const ColorTable *colors, int id)
{
	auto item = colors->find(id);

	if (item != colors->end())
		return item->second;

	return {};
}

/**
 * @brief A method to be implemented in the inheriting class. It calculates
 *	  the distance between the position of the click and the shape. This
 *	  distance is used by the GUI to decide if the corresponding
 *	  "Double click" action of the shape must be executed. The default
 *	  implementation returns infinity.
 *
 * @param x: The X coordinate of the click.
 * @param y: The Y coordinate of the click.
 */
double PlotObject::distance([[maybe_unused]] int x,
			    [[maybe_unused]] int y) const
{
	return std::numeric_limits<double>::max();
}

/**
 * @brief Create a default Shape.
 */
Shape::Shape()
: _nPoints(0),
  _points(nullptr)
{}

/**
 * @brief Create a Shape defined by "n" points. All points are initialized
 * at (0, 0).
 */
Shape::Shape(int n)
: _nPoints(n),
  _points(new(std::nothrow) ksplot_point[n]())
{
	if (!_points) {
		_size = 0;
		fprintf(stderr,
			"Failed to allocate memory for ksplot_points.\n");
	}
}

/** Copy constructor. */
Shape::Shape(const Shape &s)
: _nPoints(0),
  _points(nullptr)
{
	*this = s;
}

/** Move constructor. */
Shape::Shape(Shape &&s)
: _nPoints(s._nPoints),
  _points(s._points)
{
	s._nPoints = 0;
	s._points = nullptr;
}

/**
* @brief Destroy the Shape object.
*/
Shape::~Shape() {
	delete[] _points;
}

/** @brief Get the coordinates of the geometrical center of this shape. */
ksplot_point Shape::center() const
{
	ksplot_point c = {0, 0};

	if (_nPoints == 0)
		return c;

	for (size_t i = 0; i < _nPoints; ++i) {
		c.x += _points[i].x;
		c.y += _points[i].y;
	}

	c.x /= _nPoints;
	c.y /= _nPoints;

	return c;
}

/** Assignment operator. */
void Shape::operator=(const Shape &s)
{
	PlotObject::operator=(s);

	if (s._nPoints != _nPoints) {
		delete[] _points;
		_points = new(std::nothrow) ksplot_point[s._nPoints];
	}

	if (_points) {
		_nPoints = s._nPoints;
		memcpy(_points, s._points,
		       sizeof(_points) * _nPoints);
	} else {
		_nPoints = 0;
		fprintf(stderr,
			"Failed to allocate memory for ksplot_points.\n");
	}
}

/**
 * @brief Set the point of the polygon indexed by "i".
 *
 * @param i: the index of the point to be set.
 * @param x: X coordinate of the point in pixels.
 * @param y: Y coordinate of the point in pixels.
 */
void Shape::setPoint(size_t i, int x, int y)
{
	if (i < _nPoints) {
		_points[i].x = x;
		_points[i].y = y;
	}
}

/**
 * @brief Set the point of the polygon indexed by "i".
 *
 * @param i: the index of the point to be set.
 * @param p: A ksplot_point object used to provide coordinate values.
 */
void Shape::setPoint(size_t i, const ksplot_point &p)
{
	setPoint(i, p.x, p.y);
}

/**
 * @brief Set the point of the polygon indexed by "i".
 *
 * @param i: the index of the point to be set.
 * @param p: A Point object used to provide coordinate values.
 */
void Shape::setPoint(size_t i, const Point &p)
{
	setPoint(i, p.x(), p.y());
}

/**
 * @brief Get the point "i". If the point does not exist, the function returns
 *	  nullptr.
 */
const ksplot_point *Shape::point(size_t i) const
{
	if (i < _nPoints)
		return &_points[i];

	return nullptr;
}

/**
 * @brief Set the horizontal coordinate of the point "i".
 *
 * @param i: the index of the point to be set.
 * @param x: X coordinate of the point in pixels.
 */
void Shape::setPointX(size_t i, int x) {
	if (i < _nPoints)
		_points[i].x = x;
}

/**
 * @brief Set the vertical coordinate of the point "i".
 *
 * @param i: the index of the point to be set.
 * @param y: Y coordinate of the point in pixels.
 */
void Shape::setPointY(size_t i, int y) {
	if (i < _nPoints)
		_points[i].y = y;
}

/**
 * @brief Get the horizontal coordinate of the point "i". If the point does
 * 	  not exist, the function returns 0.
 */
int Shape::pointX(size_t i) const {
	if (i < _nPoints)
		return _points[i].x;

	return 0;
}

/**
 * @brief Get the vertical coordinate of the point "i". If the point does
 * 	  not exist, the function returns 0.
 */
int Shape::pointY(size_t i) const {
	if (i < _nPoints)
		return _points[i].y;

	return 0;
}

/** @brief Create a default Point. The point is initialized at (0, 0). */
Point::Point()
: Shape(1)
{}

/**
 * @brief Create a point.
 *
 * @param x: X coordinate of the point in pixels.
 * @param y: Y coordinate of the point in pixels.
 */
Point::Point(int x, int y)
: Shape(1)
{
	setPoint(0, x, y);
}

void Point::_draw(const Color &col, float size) const
{
	if (_nPoints == 1)
		ksplot_draw_point(_points, col.color_c_ptr(), size);
}

/**
 * @brief Draw a line between point "a" and point "b".
 *
 * @param a: The first finishing point of the line.
 * @param b: The second finishing point of the line.
 * @param col: The color of the line.
 * @param size: The size of the line.
 */
void drawLine(const Point &a, const Point &b,
	      const Color &col, float size)
{
	ksplot_draw_line(a.point_c_ptr(),
			 b.point_c_ptr(),
			 col.color_c_ptr(),
			 size);
}

/**
 * @brief Draw a dashed line between point "a" and point "b".
 *
 * @param a: The first finishing point of the line.
 * @param b: The second finishing point of the line.
 * @param col: The color of the line.
 * @param size: The size of the line.
 * @param period: The period of the dashed line.
 */
void drawDashedLine(const Point &a, const Point &b,
		    const Color &col, float size, float period)
{
	int dx = b.x() - a.x(), dy = b.y() - a.y();
	float mod = sqrt(dx * dx + dy * dy);
	int n = mod / period;
	Point p1, p2;

	for (int i = 0; i < n; ++i) {
		p1.setX(a.x() + (i + .25) * dx / n);
		p1.setY(a.y() + (i + .25) * dy / n);
		p2.setX(a.x() + (i + .75) * dx / n);
		p2.setY(a.y() + (i + .75) * dy / n);
		drawLine(p1, p2, col, size);
	}
}

/** @brief Create a default line. The two points are initialized at (0, 0). */
Line::Line()
: Shape(2)
{}

/**
 * @brief Create a line between the point "a" and point "b".
 *
 * @param a: first finishing point of the line.
 * @param b: second finishing point of the line.
 */
Line::Line(const Point &a, const Point &b)
: Shape(2)
{
	setPoint(0, a.x(), a.y());
	setPoint(1, b.x(), b.y());
}

void Line::_draw(const Color &col, float size) const
{
	if (_nPoints == 2)
		ksplot_draw_line(&_points[0], &_points[1],
				 col.color_c_ptr(), size);
}

/**
 * @brief Create a default polyline. All points are initialized at (0, 0).
 *
 * @param n: The number of points connected by the polyline.
 */
Polyline::Polyline(size_t n)
: Shape(n)
{}

void Polyline::_draw(const Color &col, float size) const
{
	ksplot_draw_polyline(_points, _nPoints, col.color_c_ptr(), size);
}

/**
 * @brief Create a default polygon. All points are initialized at (0, 0).
 *
 * @param n: The number of edges of the polygon.
 */
Polygon::Polygon(size_t n)
: Shape(n),
  _fill(true)
{}

void Polygon::_draw(const Color &col, float size) const
{
	if (_fill)
		ksplot_draw_polygon(_points, _nPoints,
				    col.color_c_ptr(),
				    size);
	else
		ksplot_draw_polygon_contour(_points, _nPoints,
					    col.color_c_ptr(),
					    size);
}
/** The created object will print/draw only the text without the frame. */
TextBox::TextBox()
: _text(""),
  _font(nullptr)
{
	setPos(Point(0, 0));
	_box._visible = false;
}


/** The created object will print/draw only the text without the frame. */
TextBox::TextBox(ksplot_font *f)
: _text(""),
  _font(f)
{
	setPos(Point(0, 0));
	_box._visible = false;
}

/** The created object will print/draw only the text without the frame. */
TextBox::TextBox(ksplot_font *f, const std::string &text, const Point &pos)
: _text(text),
  _font(f)
{
	setPos(pos);
	_box._visible = false;
}

/** The created object will print/draw only the text without the frame. */
TextBox::TextBox(ksplot_font *f, const std::string &text, const Color &col,
		 const Point &pos)
: _text(text),
  _font(f)
{
	_color = col;
	setPos(pos);
	_box._visible = false;
}

/** The created object will print/draw the text and the frame. */
TextBox::TextBox(ksplot_font *f, const std::string &text, const Color &col,
		 const Point &pos, int l, int h)
: _text(text),
  _font(f)
{
	setPos(pos);
	setBoxAppearance(col, l, h);
}

/**
 * @brief Set the position of the bottom-left corner of the frame.
 *
 * @param p: The coordinates of the bottom-left corner.
 */
void TextBox::setPos(const Point &p)
{
	_box.setPoint(0, p);
}

/**
 * @brief Set the color and the dimensions of the frame.
 *
 * @param col: The color of the frame.
 * @param l: The length of the frame.
 * @param h: The height of the frame.
 */
void TextBox::setBoxAppearance(const Color &col, int l, int h)
{
	_box.setFill(true);
	_box._color = col;
	_box._visible = true;

	if (h <= 0 && _font)
		h = _font->height;

	_box.setPoint(1, _box.pointX(0),	_box.pointY(0) - h);
	_box.setPoint(2, _box.pointX(0) + l,	_box.pointY(0) - h);
	_box.setPoint(3, _box.pointX(0) + l,	_box.pointY(0));
}

void TextBox::_draw(const Color &col, [[maybe_unused]]float size) const
{
	_box.draw();
	if (!_font || _text.empty())
		return;

	if (_box._visible ) {
		int bShift = (_box.pointY(0) - _box.pointY(1) - _font->height) / 2;
		ksplot_print_text(_font, NULL,
				  _box.pointX(0) + _font->height / 4,
				  _box.pointY(0) - _font->base - bShift,
				  _text.c_str());
	} else {
		ksplot_print_text(_font, col.color_c_ptr(),
				  _box.pointX(0) + _font->height / 4,
				  _box.pointY(0) - _font->base,
				  _text.c_str());
	}
}

/**
 * @brief Create a default Mark.
 */
Mark::Mark()
: _dashed(false)
{
	_visible = false;
	_cpu._color = Color(225, 255, 100);
	_cpu._size = 5.5f;
	_task._color = Color(0, 255, 0);
	_task._size = 5.5f;
	_combo._color = Color(100, 150, 255);
	_combo._size = 5.5f;
}

void Mark::_draw(const Color &col, float size) const
{
	if (_dashed)
		drawDashedLine(_a, _b, col, size, 3 * _cpu._size / size);
	else
		drawLine(_a, _b, col, size);

	_cpu.draw();
	_task.draw();
	_combo.draw();
}

/**
 * @brief Set the device pixel ratio.
 *
 * @param dpr: device pixel ratio value.
 */
void Mark::setDPR(int dpr)
{
	_size = 1.5 * dpr;
	_task._size = _cpu._size = _combo._size = 1.5 + 4.0 * dpr;
}

/**
 * @brief Set the X coordinate (horizontal) of the Mark.
 *
 * @param x: X coordinate of the Makr in pixels.
 */
void Mark::setX(int x)
{
	_a.setX(x);
	_b.setX(x);
	_cpu.setX(x);
	_task.setX(x);
	_combo.setX(x);
}

/**
 * @brief Set the Y coordinates (vertical) of the Mark's finishing points.
 *
 * @param yA: Y coordinate of the first finishing point of the Mark's line.
 * @param yB: Y coordinate of the second finishing point of the Mark's line.
 */
void Mark::setY(int yA, int yB)
{
	_a.setY(yA);
	_b.setY(yB);
}

/**
 * @brief Set the Y coordinates (vertical) of the Mark's CPU points.
 *
 * @param yCPU: Y coordinate of the Mark's CPU point.
 */
void Mark::setCPUY(int yCPU)
{
	_cpu.setY(yCPU);
}

/**
 * @brief Set the visiblity of the Mark's CPU points.
 *
 * @param v: If True, the CPU point will be visible.
 */
void Mark::setCPUVisible(bool v)
{
	_cpu._visible = v;
}

/**
 * @brief Set the Y coordinates (vertical) of the Mark's Task points.
 *
 * @param yTask: Y coordinate of the Mark's Task point.
 */
void Mark::setTaskY(int yTask)
{
	_task.setY(yTask);
}

/**
 * @brief Set the visiblity of the Mark's Task points.
 *
 * @param v: If True, the Task point will be visible.
 */
void Mark::setTaskVisible(bool v)
{
	_task._visible = v;
}

/**
 * @brief Set the visiblity of the Mark's Combo point.
 *
 * @param v: If True, the Task point will be visible.
 */
void Mark::setComboVisible(bool v)
{
	_combo._visible = v;
}

/**
 * @brief Set the Y coordinates (vertical) of the Mark's Combo points.
 *
 * @param yCombo: Y coordinate of the Mark's Task point.
 */
void Mark::setComboY(int yCombo)
{
	_combo.setY(yCombo);
}

/**
 * @brief Create a default Bin.
 */
Bin::Bin()
: _idFront(KS_EMPTY_BIN),
  _idBack(KS_EMPTY_BIN)
{}

void Bin::_draw(const Color &col, float size) const
{
	drawLine(_base, _val, col, size);
}

/**
 * @brief Draw only the "val" Point og the Bin.
 *
 * @param size: The size of the point.
 */
void Bin::drawVal(float size)
{
	_val._size = size;
	_val.draw();
}

/**
 * @brief Create a default (empty) Graph.
 */
Graph::Graph()
: _histoPtr(nullptr),
  _bins(nullptr),
  _size(0),
  _collectionPtr(nullptr),
  _binColors(nullptr),
  _ensembleColors(nullptr),
  _label(),
  _idleSuppress(false),
  _idlePid(0),
  _drawBase(true)
{}

/**
 * @brief Create a Graph to represent the state of the Vis. model.
 *
 * @param histo: Input location for the model descriptor.
 * @param bct: Input location for the Hash table of bin's colors.
 * @param ect: Input location for the Hash table of ensemble's colors.
 */
Graph::Graph(kshark_trace_histo *histo, KsPlot::ColorTable *bct, KsPlot::ColorTable *ect)
: _histoPtr(histo),
  _bins(new(std::nothrow) Bin[histo->n_bins]),
  _size(histo->n_bins),
  _collectionPtr(nullptr),
  _binColors(bct),
  _ensembleColors(ect),
  _label(),
  _idleSuppress(false),
  _idlePid(0),
  _drawBase(true)
{
	if (!_bins) {
		_size = 0;
		fprintf(stderr, "Failed to allocate memory graph's bins.\n");
	}

	_initBins();
}

/**
 * @brief Destroy the Graph object.
 */
Graph::~Graph()
{
	delete[] _bins;
}

int Graph::_firstBinOffset()
{
	return _labelSize + 2 * _hMargin;
}

void Graph::_initBins()
{
	for (int i = 0; i < _size; ++i) {
		_bins[i]._base.setX(i + _firstBinOffset());
		_bins[i]._base.setY(0);
		_bins[i]._val.setX(_bins[i]._base.x());
		_bins[i]._val.setY(_bins[i]._base.y());
	}
}

/**
 *  Get the number of bins.
 */
int Graph::size() const
{
	return _size;
}

/**
 * @brief Reinitialize the Graph according to the Vis. model.
 *
 * @param histo: Input location for the model descriptor.
 */
void Graph::setModelPtr(kshark_trace_histo *histo)
{
	if (_size != histo->n_bins) {
		delete[] _bins;
		_size = histo->n_bins;
		_bins = new(std::nothrow) Bin[_size];
		if (!_bins) {
			_size = 0;
			fprintf(stderr,
				"Failed to allocate memory graph's bins.\n");
		}
	}

	_histoPtr = histo;
	_initBins();
}

/**
 * @brief This function will set the Y (vertical) coordinate of the Graph's
 *	  base. It is safe to use this function even if the Graph contains
 *	  data.
 *
 * @param b: Y coordinate of the Graph's base in pixels.
 */
void Graph::setBase(int b)
{
	int mod;

	if (!_size)
		return;

	if (b == _bins[0]._base.y()) // Nothing to do.
		return;

	_label.setBoxAppearance(_label._color, _labelSize, height());

	for (int i = 0; i < _size; ++i) {
		mod = _bins[i].mod();
		_bins[i]._base.setY(b);
		_bins[i]._val.setY(b + mod);
	}
}

/**
 * @brief Set the vertical size (height) of the Graph.
 *
 * @param h: the height of the Graph in pixels.
 */
void Graph::setHeight(int h)
{
	_height = h;
}

/**
 * @brief Set the color and the dimensions of the graph's label.
 *
 * @param f: The font to be used to draw the labels.
 * @param col: The color of the ComboGraph's label.
 * @param lSize: the size of the graph's label in pixels.
 * @param hMargin: the size of the white margine space in pixels.
 */
void Graph::setLabelAppearance(ksplot_font *f, Color col, int lSize, int hMargin)
{
	if (!_size)
		return;

	_labelSize = lSize;
	_hMargin = hMargin;

	_label.setPos({_hMargin, base()});
	_label.setFont(f);
	_label.setBoxAppearance(col, lSize, height());

	for (int i = 0; i < _size; ++i) {
		_bins[i]._base.setX(i + _firstBinOffset());
		_bins[i]._val.setX(_bins[i]._base.x());
	}
}

/**
 * @brief Set Idle Suppression. If True, the bins containing Idle task records
 *	  are not grouped together.
 *
 * @param is: If True, Idle is suppressed.
 * @param ip: The process Id of the Idle task. If Idle is not suppressed, this
 *	      value has no effect.
 */
void Graph::setIdleSuppressed(bool is, int ip)
{
	_idleSuppress = is;
	_idlePid = ip;
}

/**
 * @brief Set the value of a given bin.
 *
 * @param bin: Bin Id.
 * @param val: Bin height in pixels.
 */
void Graph::setBinValue(int bin, int val)
{
	_bins[bin].setVal(val);
}

/**
 * @brief Set the Process Id (Front and Back) of a given bin.
 *
 * @param bin: Bin Id.
 * @param pidF: The Process Id detected at the from (first in time) edge of
 *		the bin.
 * @param pidB: The Process Id detected at the back (last in time) edge of
 *		the bin.
 */
void Graph::setBinPid(int bin, int pidF, int pidB)
{
	_bins[bin]._idFront = pidF;
	_bins[bin]._idBack = pidB;
}

/**
 * @brief Set the color of a given bin.
 *
 * @param bin: Bin Id.
 * @param col: the color of the bin.
 */
void Graph::setBinColor(int bin, const Color &col)
{
	_bins[bin]._color = col;
}

/**
 * @brief Set the visiblity mask of a given bin.
 *
 * @param bin: Bin Id.
 * @param m: the visiblity mask.
 */
void Graph::setBinVisMask(int bin, uint8_t m)
{
	_bins[bin]._visMask = m;
}

/**
 * @brief Set all fields of a given bin.
 *
 * @param bin: Bin Id.
 * @param pidF: The Process Id detected at the from (first in time) edge of
 *		the bin.
 * @param pidB: The Process Id detected at the back (last in time) edge of
 *		the bin.
 * @param col: the color of the bin.
 * @param m: the visiblity mask.
 */
void Graph::setBin(int bin, int pidF, int pidB, const Color &col, uint8_t m)
{
	setBinPid(bin, pidF, pidB);
	setBinValue(bin, _height * .7);
	setBinColor(bin, col);
	setBinVisMask(bin, m);
}

/**
 * @brief Process a CPU Graph.
 *
 * @param sd: Data stream identifier.
 * @param cpu: The CPU core.
 */
void Graph::fillCPUGraph(int sd, int cpu)
{
	struct kshark_entry *eFront;
	int pidFront(0), pidBack(0);
	int pidBackNoFilter;
	uint8_t visMask;
	ssize_t index;
	int bin;

	auto lamGetPid = [&] (int bin)
	{
		eFront = nullptr;

		pidFront = ksmodel_get_pid_front(_histoPtr, bin,
							    sd,
							    cpu,
							    true,
							    _collectionPtr,
							    &index);

		if (index >= 0)
			eFront = _histoPtr->data[index];

		pidBack = ksmodel_get_pid_back(_histoPtr, bin,
							  sd,
							  cpu,
							  true,
							  _collectionPtr,
							  nullptr);

		pidBackNoFilter =
			ksmodel_get_pid_back(_histoPtr, bin,
							sd,
							cpu,
							false,
							_collectionPtr,
							nullptr);

		if (pidBack != pidBackNoFilter)
			pidBack = KS_FILTERED_BIN;

		visMask = 0x0;
		if (eFront) {
			if (!(eFront->visible & KS_EVENT_VIEW_FILTER_MASK) &&
			    ksmodel_cpu_visible_event_exist(_histoPtr, bin,
								       sd,
								       cpu,
								       _collectionPtr,
								       &index)) {
				visMask = _histoPtr->data[index]->visible;
			} else {
				visMask = eFront->visible;
			}
		}
	};

	auto lamSetBin = [&] (int bin)
	{
		if (pidFront != KS_EMPTY_BIN || pidBack != KS_EMPTY_BIN) {
			/* This is a regular process. */
			setBin(bin, pidFront, pidBack,
			       getColor(_binColors, pidFront), visMask);
		} else {
			/* The bin contens no data from this CPU. */
			setBinPid(bin, KS_EMPTY_BIN, KS_EMPTY_BIN);
		}
	};

	/*
	 * Check the content of the very first bin and see if the CPU is
	 * active.
	 */
	bin = 0;
	lamGetPid(bin);
	if (pidFront >= 0) {
		/*
		 * The CPU is active and this is a regular process.
		 * Set this bin.
		 */
		lamSetBin(bin);
	} else {
		/*
		 * No data from this CPU in the very first bin. Use the Lower
		 * Overflow Bin to retrieve the Process Id (if any). First
		 * get the Pid back, ignoring the filters.
		 */
		pidBackNoFilter = ksmodel_get_pid_back(_histoPtr,
						       LOWER_OVERFLOW_BIN,
						       sd,
						       cpu,
						       false,
						       _collectionPtr,
						       nullptr);

		/* Now get the Pid back, applying filters. */
		pidBack = ksmodel_get_pid_back(_histoPtr,
					       LOWER_OVERFLOW_BIN,
					       sd,
					       cpu,
					       true,
					       _collectionPtr,
					       nullptr);

		if (pidBack != pidBackNoFilter) {
			/* The Lower Overflow Bin ends with filtered data. */
			setBinPid(bin, KS_FILTERED_BIN, KS_FILTERED_BIN);
		} else {
			/*
			 * The Lower Overflow Bin ends with data which has
			 * to be plotted.
			 */
			setBinPid(bin, pidBack, pidBack);
		}
	}

	/*
	 * The first bin is already processed. The loop starts from the second
	 * bin.
	 */
	for (bin = 1; bin < _histoPtr->n_bins; ++bin) {
		/*
		 * Check the content of this bin and see if the CPU is active.
		 * If yes, retrieve the Process Id.
		 */
		lamGetPid(bin);
		lamSetBin(bin);
	}
}

/**
 * @brief Process a Task Graph.
 *
 * @param sd: Data stream identifier.
 * @param pid: The Process Id of the Task.
 */
void Graph::fillTaskGraph(int sd, int pid)
{
	int cpuFront, cpuBack(0), pidFront(0), pidBack(0), lastCpu(-1), bin(0);
	struct kshark_entry *eFront;
	uint8_t visMask;
	ssize_t index;

	auto lamSetBin = [&] (int bin)
	{
		if (cpuFront >= 0) {
			KsPlot::Color col = getColor(_binColors, pid);

			/* Data from the Task has been found in this bin. */
			if (pid == pidFront && pid == pidBack) {
				/* No data from other tasks in this bin. */
				setBin(bin, cpuFront, cpuBack, col, visMask);
			} else if (pid != pidFront && pid != pidBack) {
				/*
				 * There is some data from other tasks at both
				 * front and back sides of this bin. But we
				 * still want to see this bin drawn.
				 */
				setBin(bin, cpuFront, KS_FILTERED_BIN, col,
				       visMask);
			} else {
				if (pidFront != pid) {
					/*
					 * There is some data from another
					 * task at the front side of this bin.
					 */
					cpuFront = KS_FILTERED_BIN;
				}

				if (pidBack != pid) {
					/*
					 * There is some data from another
					 * task at the back side of this bin.
					 */
					cpuBack = KS_FILTERED_BIN;
				}

				setBin(bin, cpuFront, cpuBack, col, visMask);
			}

			lastCpu = cpuBack;
		} else {
			/*
			 * No data from the Task in this bin. Check the CPU,
			 * previously used by the task. We are looking for
			 * data from another task running on the same CPU,
			 * hence we cannot use the collection of this task.
			 */
			int cpuPid = ksmodel_get_pid_back(_histoPtr,
							  bin,
							  sd,
							  lastCpu,
							  false,
							  nullptr, // No collection
							  nullptr);

			if (cpuPid != KS_EMPTY_BIN) {
				/*
				 * If the CPU is active and works on another
				 * task break the graph here.
				 */
				setBinPid(bin, KS_FILTERED_BIN, KS_EMPTY_BIN);
			} else {
				/*
				 * No data from this CPU in the bin.
				 * Continue the graph.
				 */
				setBinPid(bin, KS_EMPTY_BIN, KS_EMPTY_BIN);
			}
		}
	};

	auto lamGetPidCPU = [&] (int bin)
	{
		eFront = nullptr;
		/* Get the CPU used by this task. */
		cpuFront = ksmodel_get_cpu_front(_histoPtr, bin,
						 sd,
						 pid,
						 false,
						 _collectionPtr,
						 &index);
		if (cpuFront >= 0 && index >= 0)
			eFront = _histoPtr->data[index];

		cpuBack = ksmodel_get_cpu_back(_histoPtr, bin,
					       sd,
					       pid,
					       false,
					       _collectionPtr,
					       nullptr);

		if (cpuFront < 0) {
			pidFront = pidBack = cpuFront;
		} else {
			/*
			 * Get the process Id at the begining and at the end
			 * of the bin.
			 */
			pidFront = ksmodel_get_pid_front(_histoPtr,
							 bin,
							 sd,
							 cpuFront,
							 false,
							 _collectionPtr,
							 nullptr);

			pidBack = ksmodel_get_pid_back(_histoPtr,
						       bin,
						       sd,
						       cpuBack,
						       false,
						       _collectionPtr,
						       nullptr);

			visMask = 0x0;
			if (eFront) {
				if (!(eFront->visible & KS_EVENT_VIEW_FILTER_MASK) &&
				    ksmodel_task_visible_event_exist(_histoPtr,
								     bin,
								     sd,
								     pid,
								     _collectionPtr,
								     &index)) {
					visMask = _histoPtr->data[index]->visible;
				} else {
					visMask = eFront->visible;
				}
			}
		}
	};

	/*
	 * Check the content of the very first bin and see if the Task is
	 * active.
	 */
	lamGetPidCPU(bin);

	if (cpuFront >= 0) {
		/* The Task is active. Set this bin. */
		lamSetBin(bin);
	} else {
		/*
		 * No data from this Task in the very first bin. Use the Lower
		 * Overflow Bin to retrieve the CPU used by the task (if any).
		 */
		cpuFront = ksmodel_get_cpu_back(_histoPtr, LOWER_OVERFLOW_BIN,
						sd, pid,
						false, _collectionPtr, nullptr);
		if (cpuFront >= 0) {
			/*
			 * The Lower Overflow Bin contains data from this Task.
			 * Now look again in the Lower Overflow Bin and Bim 0
			 * and find the Pid of the last active task on the same
			 * CPU.
			 */
			int pidCpu0, pidCpuLOB;

			pidCpu0 = ksmodel_get_pid_back(_histoPtr,
						       0,
						       sd,
						       cpuFront,
						       false,
						       _collectionPtr,
						       nullptr);

			pidCpuLOB = ksmodel_get_pid_back(_histoPtr,
							 LOWER_OVERFLOW_BIN,
							 sd,
							 cpuFront,
							 false,
							 _collectionPtr,
							 nullptr);
			if (pidCpu0 < 0 && pidCpuLOB == pid) {
				/*
				 * The Task is the last one running on this
				 * CPU. Set the Pid of the bin. In this case
				 * the very first bin is empty but we derive
				 * the Process Id from the Lower Overflow Bin.
				 */
				setBinPid(bin, cpuFront, cpuFront);
				lastCpu = cpuFront;
			} else {
				setBinPid(bin, KS_EMPTY_BIN, KS_EMPTY_BIN);
			}
		}
	}

	/*
	 * The first bin is already processed. The loop starts from the second
	 * bin.
	 */
	for (bin = 1; bin < _histoPtr->n_bins; ++bin) {
		lamGetPidCPU(bin);

		/* Set the bin accordingly. */
		lamSetBin(bin);
	}
}

/**
 * @brief Draw the Graph
 *
 * @param size: The size of the lines of the individual Bins.
 */
void Graph::draw(float size)
{
	int lastPid(-1), b(0), boxH(_height * .3);
	Rectangle taskBox;

	_label.draw();

	if (_drawBase) {
		/*
		 * Start by drawing a line between the base points of the first and
		 * the last bin.
		 */
		drawLine(_bins[0]._base, _bins[_size - 1]._base, {}, size);
	}

	/* Draw as vartical lines all bins containing data. */
	for (int i = 0; i < _size; ++i)
		if (_bins[i]._idFront >= 0 || _bins[i]._idBack >= 0 ||
		    _bins[i]._idFront == _idlePid || _bins[i]._idBack ==_idlePid)
			if (_bins[i]._visMask & KS_EVENT_VIEW_FILTER_MASK) {
				_bins[i]._size = size;
				_bins[i].draw();
			}

	auto lamCheckEnsblVal = [this] (int v) {
		if (_idleSuppress && v == _idlePid)
			return false;

		return v >= 0;
	};

	/*
	 * Draw colored boxes for processes. First find the first bin, which
	 * contains data and determine its PID.
	 */
	for (; b < _size; ++b) {
		if (lamCheckEnsblVal(_bins[b]._idBack)) {
			lastPid = _bins[b]._idFront;
			/*
			 * Initialize a box starting from this bin.
			 * The color of the taskBox corresponds to the Pid
			 * of the process.
			 */
			taskBox._color = getColor(_ensembleColors, lastPid);
			taskBox.setPoint(0, _bins[b]._base.x(),
					_bins[b]._base.y() - boxH);
			taskBox.setPoint(1, _bins[b]._base.x(),
					_bins[b]._base.y());
			break;
		}
	}

	for (; b < _size; ++b) {
		if (_bins[b]._idFront == KS_EMPTY_BIN &&
		    _bins[b]._idBack == KS_EMPTY_BIN) {
			/*
			 * This bin is empty. If a colored taskBox is already
			 * initialized, it will be extended.
			 */
			continue;
		}

		if (_bins[b]._idFront != _bins[b]._idBack ||
		    _bins[b]._idFront != lastPid ||
		    _bins[b]._idBack  != lastPid) {
			/* A new process starts here. */
			if (b > 0 && lamCheckEnsblVal(lastPid)) {
				/*
				 * There is another process running up to this
				 * point. Close its colored box here and draw.
				 */
				taskBox.setPoint(3, _bins[b]._base.x() - 1,
						_bins[b]._base.y() - boxH);
				taskBox.setPoint(2, _bins[b]._base.x() - 1,
						_bins[b]._base.y());
				taskBox.draw();
			}

			if (lamCheckEnsblVal(_bins[b]._idBack)) {
				/*
				 * This is a regular process. Initialize
				 * colored box starting from this bin.
				 */
				taskBox._color = getColor(_ensembleColors,
							 _bins[b]._idBack);

				taskBox.setPoint(0, _bins[b]._base.x() - 1,
						_bins[b]._base.y() - boxH);
				taskBox.setPoint(1, _bins[b]._base.x() - 1,
						_bins[b]._base.y());
			}

			lastPid = _bins[b]._idBack;
		}
	}

	if (lamCheckEnsblVal(lastPid) > 0) {
		/*
		 * This is the end of the Graph and we have a process running.
		 * Close its colored box and draw.
		 */
		taskBox.setPoint(3, _bins[_size - 1]._base.x(),
				_bins[_size - 1]._base.y() - boxH);
		taskBox.setPoint(2, _bins[_size - 1]._base.x(),
				_bins[_size - 1]._base.y());
		taskBox.draw();
	}
}

void VirtGap::_draw([[maybe_unused]]const Color &col,
		    [[maybe_unused]] float size) const
{
	if (_entryPoint.x() - _exitPoint.x() < 4)
		return;

	Point p0(_exitPoint.x() + _size, _exitPoint.y());
	Point p1(_exitPoint.x() + _size, _exitPoint.y() - _height);
	Point p2(_entryPoint.x() - _size , _entryPoint.y());
	Point p3(_entryPoint.x() - _size , _entryPoint.y() - _height);

	Rectangle g;

	g.setPoint(0, p0);
	g.setPoint(1, p1);
	g.setPoint(2, p2);
	g.setPoint(3, p3);

	g._color = {255, 255, 255}; // The virt. gap is always white.
	g.setFill(false);
	g.draw();
}

} // KsPlot
