#!/usr/bin/env modelgen

import math
import list
import vec
import mat


matrix_stack = [mat.mat4(1)]

func get_matrix()
	return matrix_stack[-1]

func set_matrix(matrix)
	matrix_stack[-1] = matrix

func push()
	list.add(matrix_stack, get_matrix())

func pop()
	assert len(matrix_stack) > 1
	delete matrix_stack[-1]

func scale(x, y, z)
	set_matrix(mat.mul(get_matrix(), mat.scaling((x, y, z))))

func translate(x, y, z)
	set_matrix(mat.mul(get_matrix(), mat.translation((x, y, z))))

func rotate(radians, x, y, z)
	set_matrix(mat.mul(get_matrix(), mat.rotation(radians, vec.normalize((x, y, z)))))


proc vertex(position, normal)
	m = get_matrix()
	position = mat.mul(m, position)
	normal = mat.mul(mat.get_rotation(m), normal)
	emit position[0], position[1], position[2], normal[0], normal[1], normal[2]


func flip_triangle(p1, p2, p3)
	return p1, p3, p2

func get_triangle_normal(p1, p2, p3)
	return vec.cross(vec.sub(p2, p1), vec.sub(p3, p1))

func triangles_to_points(triangles)
	points = []
	for triangle in triangles
		assert len(triangle) == 3
		list.add_from(points, triangle)
	return points

func quads_to_triangles(quads)
	_triangles = []
	for p1, p2, p3, p4 in quads
		list.add_from(_triangles, make_quad(p1, p2, p3, p4))
	return _triangles

func translate_triangles(triangles, center)
	for triangle in triangles
		assert len(triangle) == 3
		for i in range(3)
			triangle[i] = vec.add(triangle[i], center)
	return triangles


func get_bounds(triangles)
	assert len(triangles) > 0
	min, max = triangles[0][0], triangles[0][0]
	for triangle in triangles
		assert len(triangle) == 3
		for p in triangle
			min, max = vec.min(min, p), vec.max(max, p)
	return vec.sub(max, min)

func get_center_point(polygon)
	assert len(polygon) > 0
	center = (0,) * len(polygon[0])
	for p in polygon
		center = vec.add(center, p)
	center = vec.div(center, len(polygon))
	return center

func is_convex(polygon)
	assert len(polygon) > 2
	for i in range(len(polygon))
		p1 = polygon[i]
		p2 = polygon[(i + 1) % len(polygon)]
		p3 = polygon[(i + 2) % len(polygon)]
		d1, d2 = vec.sub(p3, p2), vec.sub(p1, p2)
		assert len(d1) == 2 and len(d2) == 2
		z = d1[0] * d2[1] - d1[1] * d2[0]
		if i == 0
			sign = z > 0
		else if sign != (z > 0)
			return false
	return true


func triangulate(polygon, center = (0, 0, 0), clockwise = false)
	assert len(polygon) > 2
	_triangles = []
	if len(polygon) == 3
		p1, p2, p3 = polygon
		list.add(_triangles, (p1, p2, p3))
	else if len(polygon) == 4
		p1, p2, p3, p4 = polygon
		list.add(_triangles, (p1, p2, p4), (p2, p3, p4))
	else if is_convex(polygon)
		p1 = polygon[0]
		for i in range(len(polygon))
			p2, p3 = polygon[(i + 1) % len(polygon)], polygon[(i + 2) % len(polygon)]
			list.add(_triangles, (p1, p2, p3))
	else
		assert false, "Unsupported"
	for i in range(len(_triangles))
		assert len(_triangles[i]) == 3
		for j in range(3)
			_triangles[i][j] = vec.add((_triangles[i][j][0], 0, _triangles[i][j][1]), center)
		if clockwise
			_triangles[i] = flip_triangle(_triangles[i][0], _triangles[i][1], _triangles[i][2])
	assert len(_triangles) > 0
	return _triangles


# 1 +--+ 4
#   | /|
#   |/ |
# 2 +--+ 3
func make_quad(p1, p2, p3, p4, center = (0, 0, 0))
	p1, p2 = vec.add(p1, center), vec.add(p2, center)
	p3, p4 = vec.add(p3, center), vec.add(p4, center)
	return (p1, p2, p4), (p2, p3, p4)


func make_oblique_pyramid(polygon, translation = (0, 1, 0), center = (0, 0, 0))
	assert len(polygon) > 2
	_triangles = []
	half_translation = vec.div(translation, 2)
	top = get_center_point(polygon)
	top = vec.add((top[0], 0, top[1]), half_translation)
	top = vec.add(top, center)
	bottom = triangulate(polygon, center, true)
	for i in range(len(bottom))
		assert len(bottom[i]) == 3
		for j in range(3)
			bottom[i][j] = vec.sub(bottom[i][j], half_translation)
		list.add(_triangles, bottom[i])
	for i in range(len(polygon))
		p2, p3 = polygon[(i + 1) % len(polygon)], polygon[(i + 2) % len(polygon)]
		p2 = p2[0], 0, p2[1]
		p3 = p3[0], 0, p3[1]
		p2, p3 = vec.sub(p2, half_translation), vec.sub(p3, half_translation)
		p2, p3 = vec.add(p2, center), vec.add(p3, center)
		list.add(_triangles, (top, p2, p3))
	return _triangles

func make_pyramid(polygon, height, center = (0, 0, 0))
	return make_oblique_pyramid(polygon, (0, height, 0), center)


func make_oblique_frustum(top, bottom, translation = (0, 1, 0), center = (0, 0, 0))
	assert len(top) > 2 and len(bottom) > 2 and len(top) == len(bottom)
	_triangles = []
	half_translation = vec.div(translation, 2)
	_top, _bottom = triangulate(top, center), triangulate(bottom, center, true)
	for i in range(len(_top))
		assert len(_top[i]) == 3
		for j in range(3)
			_top[i][j] = vec.add(_top[i][j], half_translation)
		list.add(_triangles, _top[i])
	for i in range(len(_bottom))
		assert len(_bottom[i]) == 3
		for j in range(3)
			_bottom[i][j] = vec.sub(_bottom[i][j], half_translation)
		list.add(_triangles, _bottom[i])
	for i in range(len(top))
		p1, p4 = top[i], top[(i + 1) % len(top)]
		p2, p3 = bottom[i], bottom[(i + 1) % len(bottom)]
		p1 = p1[0], 0, p1[1]
		p2 = p2[0], 0, p2[1]
		p3 = p3[0], 0, p3[1]
		p4 = p4[0], 0, p4[1]
		p1, p4 = vec.add(p1, half_translation), vec.add(p4, half_translation)
		p2, p3 = vec.sub(p2, half_translation), vec.sub(p3, half_translation)
		list.add_from(_triangles, make_quad(p1, p2, p3, p4, center))
	return _triangles

func make_frustum(top, bottom, height = 1, center = (0, 0, 0))
	return make_oblique_frustum(top, bottom, (0, height, 0), center)


func make_oblique_prism(polygon, translation = (0, 1, 0), center = (0, 0, 0))
	assert len(polygon) > 2
	_triangles = []
	half_translation = vec.div(translation, 2)
	bottom, top = triangulate(polygon, center, true), triangulate(polygon, center)
	for i in range(len(bottom))
		assert len(bottom[i]) == 3
		for j in range(3)
			bottom[i][j] = vec.sub(bottom[i][j], half_translation)
		list.add(_triangles, bottom[i])
	for i in range(len(top))
		assert len(top[i]) == 3
		for j in range(3)
			top[i][j] = vec.add(top[i][j], half_translation)
		list.add(_triangles, top[i])
	for i in range(len(polygon))
		p2, p3 = polygon[i], polygon[(i + 1) % len(polygon)]
		p2 = p2[0], 0, p2[1]
		p3 = p3[0], 0, p3[1]
		p2, p3 = vec.sub(p2, half_translation), vec.sub(p3, half_translation)
		p1, p4 = vec.add(p2, translation), vec.add(p3, translation)
		list.add_from(_triangles, make_quad(p1, p2, p3, p4, center))
	return _triangles

func make_prism(polygon, height = 1, center = (0, 0, 0))
	return make_oblique_prism(polygon, (0, height, 0), center)


func make_triangle(size = (1, 1), center = (0, 0))
	(hw, hh), (x, y) = vec.div(size, 2), center
	return [
		(x - hw, y - hh),
		(x - hw, y + hh),
		(x + hw, y - hh)]


func make_rectangle(size = (1, 1), center = (0, 0))
	(hw, hh), (x, y) = vec.div(size, 2), center
	return [
		(x - hw, y - hh),
		(x - hw, y + hh),
		(x + hw, y + hh),
		(x + hw, y - hh)]


func make_circle(diameter = 1, center = (0, 0), segments = 8)
	r, (x, y) = diameter / 2, center
	assert segments > 2
	angle = math.rad(360 / segments)
	polygon = []
	for i in range(segments)
		list.add(polygon, (x - math.cos(angle * i) * r, y + math.sin(angle * i) * r))
	return polygon


func make_ellipse(size = (1, 1), center = (0, 0), segments = 8)
	(w, h), (x, y) = vec.div(size, 2), center
	assert segments > 2
	angle = math.rad(360 / segments)
	polygon = []
	for i in range(segments)
		list.add(polygon, (x - math.cos(angle * i) * w, y + math.sin(angle * i) * h))
	return polygon


func make_cube(size = (1, 1, 1), center = (0, 0, 0))
	hw, hh, hl = vec.div(size, 2)
	return translate_triangles(quads_to_triangles([
		((-hw, hh, -hl), (-hw, hh, hl), (hw, hh, hl), (hw, hh, -hl)), # Top
		((-hw, -hh, hl), (-hw, -hh, -hl), (hw, -hh, -hl), (hw, -hh, hl)), # Bottom
		((-hw, hh, hl), (-hw, -hh, hl), (hw, -hh, hl), (hw, hh, hl)), # Front
		((hw, hh, -hl), (hw, -hh, -hl), (-hw, -hh, -hl), (-hw, hh, -hl)), # Back
		((-hw, hh, -hl), (-hw, -hh, -hl), (-hw, -hh, hl), (-hw, hh, hl)), # Left
		((hw, hh, hl), (hw, -hh, hl), (hw, -hh, -hl), (hw, hh, -hl))]), # Right
		center)

func make_square_pyramid(size = (1, 1, 1), center = (0, 0, 0))
	hw, hh, hl = vec.div(size, 2)
	_triangles = []
	list.add_from(_triangles, quads_to_triangles([
		((-hw, -hh, hl), (-hw, -hh, -hl), (hw, -hh, -hl), (hw, -hh, hl))])) # Bottom
	list.add_from(_triangles, [
		((-hw, -hh, hl), (hw, -hh, hl), (0, hh, 0)), # Front
		((hw, -hh, -hl), (-hw, -hh, -hl), (0, hh, 0)), # Back
		((hw, -hh, hl), (hw, -hh, -hl), (0, hh, 0)), # Left
		((-hw, -hh, -hl), (-hw, -hh, hl), (0, hh, 0))]) # Right
	return translate_triangles(_triangles, center)


proc triangle(p1, p2, p3, center = (0, 0, 0), clockwise = false)
	if clockwise
		p1, p2, p3 = flip_triangle(p1, p2, p3)
	normal = get_triangle_normal(p1, p2, p3)
	for position in p1, p2, p3
		vertex(vec.add(position, center), normal)

proc triangles(triangles, center = (0, 0, 0), clockwise = false)
	for p1, p2, p3 in triangles
		triangle(p1, p2, p3, center, clockwise)


proc quad(p1, p2, p3, p4, clockwise = false)
	triangles(make_quad(p1, p2, p3, p4), clockwise)


proc cube(size = (1, 1, 1), center = (0, 0, 0), inverted = false)
	triangles(make_cube(size, center), inverted)

proc square_pyramid(size = (1, 1, 1), center = (0, 0, 0), inverted = false)
	triangles(make_square_pyramid(size, center), inverted)


proc oblique_pyramid(polygon, translation = (0, 1, 0), center = (0, 0, 0))
	triangles(make_oblique_pyramid(polygon, translation, center))

proc pyramid(polygon, height = 1, center = (0, 0, 0))
	triangles(make_pyramid(polygon, height, center))


proc oblique_frustum(top, bottom, translation = (0, 1, 0), center = (0, 0, 0))
	triangles(make_oblique_frustum(top, bottom, translation, center))

proc frustum(top, bottom, height = 1, center = (0, 0, 0))
	triangles(make_frustum(top, bottom, height, center))


proc oblique_prism(polygon, translation = (0, 1, 0), center = (0, 0, 0))
	triangles(make_oblique_prism(polygon, translation, center))

proc prism(polygon, height = 1, center = (0, 0, 0))
	triangles(make_prism(polygon, height, center))

proc cylinder(diameter, height, center = (0, 0, 0), segments = 8)
	prism(make_circle(diameter, (0, 0), segments), height, center)
