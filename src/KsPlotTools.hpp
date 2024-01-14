/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2017 VMware Inc, Yordan Karadzhov <y.karadz@gmail.com>
 */

 /**
 *  @file    KsPlotTools.hpp
 *  @brief   KernelShark Plot tools.
 */

#ifndef _KS_PLOT_TOOLS_H
#define _KS_PLOT_TOOLS_H

// C++
#include <forward_list>
#include <unordered_map>
#include <string>

// KernelShark
#include "libkshark.h"
#include "libkshark-plot.h"
#include "libkshark-model.h"

namespace KsPlot {

/** This class represents a RGB color. */
class Color {
public:
	Color();

	Color(uint8_t r, uint8_t g, uint8_t b);

	Color(int rgb);

	/** @brief Get the Red coordinate of the color. */
	uint8_t r() const {return _col_c.red;}

	/** @brief Get the Green coordinate of the color. */
	uint8_t g() const {return _col_c.green;}

	/** @brief Get the Blue coordinate of the color. */
	uint8_t b() const {return _col_c.blue;}

	void set(uint8_t r, uint8_t g, uint8_t b);

	void set(int rgb);

	void setRainbowColor(int n);

	void blend(float alpha);

	/**
	 * @brief Get the C struct defining the RGB color.
	 */
	const ksplot_color *color_c_ptr() const {return &_col_c;}

	/**
	 * @brief Set the frequency value used to generate the Rainbow
	 * palette.
	 */
	static void setRainbowFrequency(float f) {_frequency = f;}

	/**
	 * @brief Get the frequency value used to generate the Rainbow
	 * palette.
	 */
	static float rainbowFrequency() {return _frequency;}

private:
	ksplot_color _col_c;

	/** The frequency value used to generate the Rainbow palette. */
	static float _frequency;
};

/** Hash table of colors. */
typedef std::unordered_map<int, KsPlot::Color> ColorTable;

ColorTable taskColorTable();

ColorTable CPUColorTable();

ColorTable streamColorTable();

Color getColor(const ColorTable *colors, int id);

/** Represents an abstract graphical element. */
class PlotObject {
public:
	/**
	 * @brief Create a default object.
	 */
	PlotObject() : _visible(true), _size(2.) {}

	/**
	 * @brief Destroy the object. Keep this destructor virtual.
	 */
	virtual ~PlotObject() {}

	/** Generic function used to draw different objects. */
	void draw() const {
		if (_visible)
			_draw(_color, _size);
	}

	/**
	 * Generic action to be executed when the objects is double clicked.
	 */
	void doubleClick() const {
		if (_visible)
			_doubleClick();
	}

	virtual double distance(int x, int y) const;

	/** Is this object visible. */
	bool	_visible;

	/** The color of the object. */
	Color	_color;

	/** The size of the object. */
	float	_size;

private:
	virtual void _draw(const Color &col, float s) const = 0;

	virtual void _doubleClick() const {}
};

/** List of graphical element. */
typedef std::forward_list<PlotObject*> PlotObjList;

class Point;

/** Represents an abstract shape. */
class Shape : public PlotObject {
public:
	Shape();

	Shape(int n);

	Shape(const Shape &);

	Shape(Shape &&);

	/* Keep this destructor virtual. */
	virtual ~Shape();

	ksplot_point center() const;

	void operator=(const Shape &s);

	void setPoint(size_t i, int x, int y);

	void setPoint(size_t i, const ksplot_point &p);

	void setPoint(size_t i, const Point &p);

	const ksplot_point *point(size_t i) const;

	void setPointX(size_t i, int x);

	void setPointY(size_t i, int y);

	int pointX(size_t i) const;

	int pointY(size_t i) const;

	/**
	 * @brief Get the number of point used to define the polygon.
	 */
	size_t pointCount() const {return _nPoints;}

protected:
	/** The number of point used to define the polygon. */
	size_t		_nPoints;

	/** The array of point used to define the polygon. */
	ksplot_point	*_points;
};

/** This class represents a 2D poin. */
class Point : public Shape {
public:
	Point();

	Point(int x, int y);

	/**
	 * @brief Destroy the Point object. Keep this destructor virtual.
	 */
	virtual ~Point() {}

	/** @brief Get the horizontal coordinate of the point. */
	int x() const {return pointX(0);}

	/** @brief Get the vertical coordinate of the point. */
	int y() const {return pointY(0);}

	/** @brief Set the horizontal coordinate of the point. */
	void setX(int x) {setPointX(0, x);}

	/** @brief Set the vertical coordinate of the point. */
	void setY(int y) {setPointY(0, y);}

	/**
	 * @brief Set the coordinats of the point.
	 *
	 * @param x: horizontal coordinate of the point in pixels.
	 * @param y: vertical coordinate of the point in pixels.
	 */
	void set(int x, int y) {setPoint(0, x, y);}

	/**
	 * @brief Get the C struct defining the point.
	 */
	const ksplot_point *point_c_ptr() const {return point(0);}

private:
	void _draw(const Color &col, float size = 1.) const override;
};

void drawLine(const Point &a, const Point &b,
	      const Color &col, float size);

void drawDashedLine(const Point &a, const Point &b,
		    const Color &col, float size, float period);

/** This class represents a straight line. */
class Line : public Shape {
public:
	Line();

	Line(const Point &a, const Point &b);

	/**
	 * @brief Destroy the Line object. Keep this destructor virtual.
	 */
	virtual ~Line() {}

	/**
	 * @brief Set the coordinats of the first finishing point of the
	 *	  line.
	 *
	 * @param x: horizontal coordinate of the point in pixels.
	 * @param y: vertical coordinate of the point in pixels.
	 */
	void setA(int x, int y) { setPoint(0, x, y);}

	/** @brief Get the first finishing point of the line. */
	const ksplot_point *a() const {return point(0);}

	/**
	 * @brief Set the coordinats of the second finishing point of the
	 *	  line.
	 *
	 * @param x: horizontal coordinate of the point in pixels.
	 * @param y: vertical coordinate of the point in pixels.
	 */
	void setB(int x, int y) {setPoint(1, x, y);}

	/** @brief Get the second finishing point of the line. */
	const ksplot_point *b() const {return point(1);}

private:
	void _draw(const Color &col, float size = 1.) const override;
};

/** This class represents a polyline. */
class Polyline : public Shape {
public:
	Polyline(size_t n);

	/**
	 * @brief Destroy the polyline object. Keep this destructor virtual.
	 */
	virtual ~Polyline() {}

private:
	Polyline() = delete;

	void _draw(const Color &, float size = 1.) const override;
};

/** This class represents a polygon. */
class Polygon : public Shape {
public:
	Polygon(size_t n);

	/**
	 * @brief Destroy the polygon object. Keep this destructor virtual.
	 */
	virtual ~Polygon() {}

	/**
	 * @brief Specify the way the polygon will be drawn.
	 *
	 * @param f: If True, the area of the polygon will be colored.
	 *	  Otherwise only the contour of the polygon will be plotted.
	 */
	void setFill(bool f) {_fill = f;}

private:
	Polygon() = delete;

	void _draw(const Color &, float size = 1.) const override;

	/**
	 * If True, the area of the polygon will be colored. Otherwise only
	 * the contour of the polygon will be plotted.
	 */
	bool		_fill;
};

/** This class represents a triangle. */
class Triangle : public Polygon {
public:
	/**
	 * Create a default triangle. All points are initialized at (0, 0).
	 */
	Triangle() : Polygon(3) {}

	/** Destroy the Triangle object. Keep this destructor virtual. */
	virtual ~Triangle() {}
};

/** This class represents a rectangle. */
class Rectangle : public Polygon {
public:
	/**
	 * Create a default Rectangle. All points are initialized at (0, 0).
	 */
	Rectangle() : Polygon(4) {}

	/** Destroy the Rectangle object. Keep this destructor virtual. */
	virtual ~Rectangle() {}
};

/**
 * This class represents a text that is printed/drawn inside a colorful frame.
 */
class TextBox : public PlotObject {
public:
	TextBox();

	TextBox(ksplot_font *f);

	TextBox(ksplot_font *f, const std::string &text, const Point &pos);

	TextBox(ksplot_font *f, const std::string &text, const Color &col,
		const Point &pos);

	TextBox(ksplot_font *f, const std::string &text, const Color &col,
		const Point &pos, int l, int h = -1);

	/** Destroy the TextBox object. Keep this destructor virtual. */
	virtual ~TextBox() {}

	/** Set the font to be used. */
	void setFont(ksplot_font *f) {_font = f;}

	/** Set the text. */
	void setText(const std::string &t) {_text = t;}

	void setPos(const Point &p);

	void setBoxAppearance(const Color &col, int l, int h);

private:
	void _draw(const Color &, float size = 1.) const override;

	std::string	_text;

	ksplot_font	*_font;

	Rectangle	_box;
};

/**
 * This class represents the graphical element of the KernelShark GUI marker.
 */
class Mark : public PlotObject {
public:
	Mark();

	/**
	 * @brief Destroy the Mark object. Keep this destructor virtual.
	 */
	virtual ~Mark() {}

	void setDPR(int dpr);

	void setX(int x);

	void setY(int yA, int yB);

	/** Get the Y coordinate of the Mark's CPU point. */
	int cpuY() const {return _cpu.y();}

	void setCPUY(int yCPU);

	/** Is the CPU point visible. */
	bool cpuIsVisible() const {return _cpu._visible;}

	void setCPUVisible(bool v);

	/** Get the Y coordinate of the Mark's Task point. */
	int taskY() const {return _task.y();}

	void setTaskY(int yTask);

	/** Is the Task point visible. */
	bool taskIsVisible() const {return _task._visible;}

	void setTaskVisible(bool v);

	/** If True, the Mark will be plotted as a dashed line. */
	void setDashed(bool d) {_dashed = d;}

	/** Get the Y coordinate of the Mark's Combo point. */
	int comboY() const {return _combo.y();}

	void setComboY(int yCombo);

	/** Is the Combo point visible. */
	bool comboIsVisible() const {return _combo._visible;}

	void setComboVisible(bool v);

private:
	void _draw(const Color &col, float size = 1.) const override;

	/** First finishing point of the Mark's line. */
	Point _a;

	/** Second finishing point of the Mark's line. */
	Point _b;

	/** A point indicating the position of the Mark in a CPU graph. */
	Point _cpu;

	/** A point indicating the position of the Mark in a Task graph. */
	Point _task;

	/** A point indicating the position of the Mark in a Combo graph. */
	Point _combo;

	/** If True, plot the Mark as a dashed line. */
	bool _dashed;
};

/** This class represents a KernelShark graph's bin. */
class Bin : public PlotObject {
public:
	Bin();

	/**
	 * @brief Destroy the Bin object. Keep this destructor virtual.
	 */
	virtual ~Bin() {}

	void drawVal(float size = 2.);

	/** Get the height (module) of the line, representing the Bin. */
	int mod() const {return _val.y() - _base.y();}

	/** @brief Set the vertical coordinate of the "val" Point. */
	void setVal(int v) {_val.setY(_base.y() - v); }

	/**
	 * The Id value (pid or cpu) detected at the front (first in time) edge
	 * of the bin.
	 */
	int	_idFront;

	/**
	 * The Id value (pid or cpu) detected at the back (last in time) edge
	 * of the bin.
	 */
	int	_idBack;

	/** Lower finishing point of the line, representing the Bin. */
	Point	_base;

	/** Upper finishing point of the line, representing the Bin. */
	Point	_val;

	/** A bit mask controlling the visibility of the Bin. */
	uint8_t	_visMask;

private:
	void _draw(const Color &col, float size = 1.) const override;
};

/** This class represents a KernelShark graph. */
class Graph {
public:
	Graph();

	/*
	 * Disable copying. We can enable the Copy Constructor in the future,
	 * but only if we really need it for some reason.
	 */
	Graph(const Graph &) = delete;

	/* Disable moving. Same as copying. */
	Graph(Graph &&) = delete;

	Graph(kshark_trace_histo *histo, KsPlot::ColorTable *bct,
					 KsPlot::ColorTable *ect);

	/* Keep this destructor virtual. */
	virtual ~Graph();

	int size() const;

	void setModelPtr(kshark_trace_histo *histo);

	/**
	 * @brief Provide the Graph with a Data Collection. The collection
	 *	  will be used to optimise the processing of the content of
	 *	  the bins.
	 *
	 * @param col: Input location for the data collection descriptor.
	 */
	void setDataCollectionPtr(kshark_entry_collection *col) {
		_collectionPtr = col;
	}

	/** @brief Set the Hash table of Task's colors. */
	void setBinColorTablePtr(KsPlot::ColorTable *ct) {_binColors = ct;}

	void fillCPUGraph(int sd, int cpu);

	void fillTaskGraph(int sd, int pid);

	void draw(float s = 1);

	void setBase(int b);

	/** @brief Get the vertical coordinate of the Graph's base. */
	int base() const {return _bins[0]._base.y();}

	void setHeight(int h);

	/** @brief Get the vertical size (height) of the Graph. */
	int height() const {return _height;}

	void setBinValue(int bin, int val);

	void setBinPid(int bin, int pidF, int pidB);

	void setBinColor(int bin, const Color &col);

	void setBinVisMask(int bin, uint8_t m);

	void setBin(int bin, int pidF, int pidB,
		    const Color &col, uint8_t m);

	/** @brief Get a particular bin. */
	const Bin &bin(int bin) const {return _bins[bin];}

	/** Set the text of the graph's label. */
	void setLabelText(std::string text) {_label.setText(text);}

	void setLabelAppearance(ksplot_font *f, Color col,
				int lSize, int hMargin);

	void setIdleSuppressed(bool is, int ip = 0);

	/** Draw the base line of the graph or not. */
	void setDrawBase(bool b) {_drawBase = b;}

protected:
	/** Pointer to the model descriptor object. */
	kshark_trace_histo	*_histoPtr;

	/** An array of Bins. */
	Bin			*_bins;

	/** The number of Bins. */
	int			_size;

	/**
	 * The size (in pixels) of the white space added on both sides of
	 * the Graph.
	 */
	int			_hMargin;

	/** The horizontal size of the Graph's label. */
	int			_labelSize;

	/** The vertical size (height) of the Graph. */
	int			_height;

	/** Pointer to the data collection object. */
	kshark_entry_collection	*_collectionPtr;

	/** Hash table of bin's colors. */
	ColorTable		*_binColors;

	/** Hash table of ensemble's colors. */
	ColorTable		*_ensembleColors;

private:
	TextBox		_label;

	bool	_idleSuppress;

	int	_idlePid;

	bool	_drawBase;

	void	_initBins();

	int	_firstBinOffset();
};

/**
 * This class represents the graphical element visualizing how the execution
 * goes from the host to the guest and back.
 */
class VirtBridge : public Polyline {
public:
	/** Create a default VirtBridge. */
	VirtBridge() : Polyline(4) {}

	/* Keep this destructor virtual. */
	virtual ~VirtBridge() {}

	/** Set the coordinates of the EntryHost point of the VirtBridge. */
	void setEntryHost(int x, int y) {setPoint(0, x, y);}

	/** Set the coordinates of the EntryGuest point of the VirtBridge. */
	void setEntryGuest(int x, int y) {setPoint(1, x, y);}

	/** Set the coordinates of the ExitGuest point of the VirtBridge. */
	void setExitGuest(int x, int y) {setPoint(2, x, y);}

	/** Set the coordinates of the ExitHost point of the VirtBridge. */
	void setExitHost(int x, int y) {setPoint(3, x, y);}
};

/**
 * This class represents the graphical element visualizing the time interval
 * in the guest during which the execution has been returned to the host.
 */
class VirtGap : public Shape {
public:
	/** Create a VirtGap with height "h". */
	VirtGap(int h) :_height(h) {}

	/**  The point where the execution exits the VM. */
	Point _exitPoint;

	/** The point where the execution enters the VM. */
	Point _entryPoint;

private:
	/** The vertical size (height) of the graphical element. */
	int _height;

	void _draw(const Color &col, float size) const override;
};

}; // KsPlot

#endif  /* _KS_PLOT_TOOLS_H */
