//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GeometricCut2DUserObject.h"

// MOOSE includes
#include "MooseError.h"

#include "libmesh/string_to_enum.h"

// XFEM includes
#include "XFEMFuncs.h"

defineLegacyParams(GeometricCut2DUserObject);

InputParameters
GeometricCut2DUserObject::validParams()
{
  InputParameters params = GeometricCutUserObject::validParams();
  params.addParam<Real>("time_start_cut", 0.0, "Start time of geometric cut propagation");
  params.addParam<Real>("time_end_cut", 0.0, "End time of geometric cut propagation");
  params.addClassDescription("Base class for 2D XFEM Geometric Cut UserObjects");
  return params;
}

GeometricCut2DUserObject::GeometricCut2DUserObject(const InputParameters & parameters)
  : GeometricCutUserObject(parameters), _cut_line_endpoints(), _cut_time_ranges()
{
  _cut_time_ranges.push_back(
      std::make_pair(getParam<Real>("time_start_cut"), getParam<Real>("time_end_cut")));
}

bool
GeometricCut2DUserObject::cutElementByGeometry(const Elem * elem,
                                               std::vector<Xfem::CutEdge> & cut_edges,
                                               std::vector<Xfem::CutNode> & cut_nodes,
                                               Real time) const
{
  bool cut_elem = false;

  for (unsigned int cut = 0; cut < _cut_line_endpoints.size(); ++cut)
  {
    Real fraction = cutFraction(cut, time);

    if (fraction > 0.0)
    {
      unsigned int n_sides = elem->n_sides();

      for (unsigned int i = 0; i < n_sides; ++i)
      {
        // This returns the lowest-order type of side, which should always
        // be an EDGE2 here because this class is for 2D only.
        std::unique_ptr<const Elem> curr_side = elem->side_ptr(i);
        if (curr_side->type() != EDGE2)
          mooseError("In cutElementByGeometry element side must be EDGE2, but type is: ",
                     libMesh::Utility::enum_to_string(curr_side->type()),
                     " base element type is: ",
                     libMesh::Utility::enum_to_string(elem->type()));

        const Node * node1 = curr_side->node_ptr(0);
        const Node * node2 = curr_side->node_ptr(1);
        Real seg_int_frac = 0.0;

        if (IntersectSegmentWithCutLine(
                *node1, *node2, _cut_line_endpoints[cut], fraction, seg_int_frac))
        {
          if (seg_int_frac > Xfem::tol && seg_int_frac < 1.0 - Xfem::tol)
          {
            cut_elem = true;
            Xfem::CutEdge mycut;
            mycut._id1 = node1->id();
            mycut._id2 = node2->id();
            mycut._distance = seg_int_frac;
            mycut._host_side_id = i;
            cut_edges.push_back(mycut);
          }
          else if (seg_int_frac < Xfem::tol)
          {
            cut_elem = true;
            Xfem::CutNode mycut;
            mycut._id = node1->id();
            mycut._host_id = i;
            cut_nodes.push_back(mycut);
          }
        }
      }
    }
  }
  return cut_elem;
}

bool
GeometricCut2DUserObject::cutElementByGeometry(const Elem * /*elem*/,
                                               std::vector<Xfem::CutFace> & /*cut_faces*/,
                                               Real /*time*/) const
{
  mooseError("Invalid method: must use vector of element edges for 2D mesh cutting");
  return false;
}

bool
GeometricCut2DUserObject::cutFragmentByGeometry(std::vector<std::vector<Point>> & frag_edges,
                                                std::vector<Xfem::CutEdge> & cut_edges,
                                                Real time) const
{
  bool cut_frag = false;

  for (unsigned int cut = 0; cut < _cut_line_endpoints.size(); ++cut)
  {
    Real fraction = cutFraction(cut, time);

    if (fraction > 0.0)
    {
      unsigned int n_sides = frag_edges.size();

      for (unsigned int i = 0; i < n_sides; ++i)
      {
        Real seg_int_frac = 0.0;

        if (IntersectSegmentWithCutLine(frag_edges[i][0],
                                        frag_edges[i][1],
                                        _cut_line_endpoints[cut],
                                        fraction,
                                        seg_int_frac))
        {
          cut_frag = true;
          Xfem::CutEdge mycut;
          mycut._id1 = i;
          mycut._id2 = (i < (n_sides - 1) ? (i + 1) : 0);
          mycut._distance = seg_int_frac;
          mycut._host_side_id = i;
          cut_edges.push_back(mycut);
        }
      }
    }
  }
  return cut_frag;
}

bool
GeometricCut2DUserObject::cutFragmentByGeometry(std::vector<std::vector<Point>> & /*frag_faces*/,
                                                std::vector<Xfem::CutFace> & /*cut_faces*/,
                                                Real /*time*/) const
{
  mooseError("Invalid method: must use vector of element edges for 2D mesh cutting");
  return false;
}

bool
GeometricCut2DUserObject::IntersectSegmentWithCutLine(
    const Point & segment_point1,
    const Point & segment_point2,
    const std::pair<Point, Point> & cutting_line_points,
    const Real & cutting_line_fraction,
    Real & segment_intersection_fraction) const
{
  // Use the algorithm described here to determine whether a line segment is intersected
  // by a cutting line, and to compute the fraction along that line where the intersection
  // occurs:
  // http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect

  bool cut_segment = false;
  Point seg_dir = segment_point2 - segment_point1;
  Point cut_dir = cutting_line_points.second - cutting_line_points.first;
  Point cut_start_to_seg_start = segment_point1 - cutting_line_points.first;

  Real cut_dir_cross_seg_dir = crossProduct2D(cut_dir, seg_dir);

  if (std::abs(cut_dir_cross_seg_dir) > Xfem::tol)
  {
    // Fraction of the distance along the cutting segment where it intersects the edge segment
    Real cut_int_frac = crossProduct2D(cut_start_to_seg_start, seg_dir) / cut_dir_cross_seg_dir;

    if (cut_int_frac >= 0.0 && cut_int_frac <= cutting_line_fraction)
    { // Cutting segment intersects the line of the edge segment, but the intersection point may
      // be
      // outside the segment
      Real int_frac = crossProduct2D(cut_start_to_seg_start, cut_dir) / cut_dir_cross_seg_dir;
      if (int_frac >= 0.0 &&
          int_frac <= 1.0) // TODO: revisit end cases for intersections with corners
      {
        cut_segment = true;
        segment_intersection_fraction = int_frac;
      }
    }
  }
  return cut_segment;
}

Real
GeometricCut2DUserObject::crossProduct2D(const Point & point_a, const Point & point_b) const
{
  return (point_a(0) * point_b(1) - point_b(0) * point_a(1));
}

Real
GeometricCut2DUserObject::cutFraction(unsigned int cut_num, Real time) const
{
  mooseAssert(_cut_time_ranges.size() > cut_num,
              "cut_num is outside the bounds of _cut_time_ranges");

  Real fraction = 0.0;

  if (time >= _cut_time_ranges[cut_num].first)
  {
    if (time >= _cut_time_ranges[cut_num].second)
      fraction = 1.0;
    else
    {
      Real denominator = _cut_time_ranges[cut_num].second - _cut_time_ranges[cut_num].first;
      if (MooseUtils::absoluteFuzzyEqual(denominator, 0.0))
        mooseError("time_start_cut and time_end_cut cannot have the same value");
      fraction = (time - _cut_time_ranges[cut_num].first) / denominator;
    }
  }
  return fraction;
}
