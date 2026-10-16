#include <litefont/Outline.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace litefont {

void Outline::moveTo(Vector2f point) {
	this->verbs.push_back(OutlineVerb::Move);
	this->points.push_back(point);
}

void Outline::lineTo(Vector2f point) {
	this->verbs.push_back(OutlineVerb::Line);
	this->points.push_back(point);
}

void Outline::quadTo(Vector2f control, Vector2f point) {
	this->verbs.push_back(OutlineVerb::Quad);
	this->points.push_back(control);
	this->points.push_back(point);
}

void Outline::cubicTo(Vector2f control1, Vector2f control2, Vector2f point) {
	this->verbs.push_back(OutlineVerb::Cubic);
	this->points.push_back(control1);
	this->points.push_back(control2);
	this->points.push_back(point);
}

void Outline::close() {
	this->verbs.push_back(OutlineVerb::Close);
}

void Outline::append(const Outline& other) {
	this->verbs.insert(this->verbs.end(), other.verbs.begin(), other.verbs.end());
	this->points.insert(this->points.end(), other.points.begin(), other.points.end());
}

void Outline::clear() {
	this->verbs.clear();
	this->points.clear();
}

Outline Outline::getTransformed(const Affine2f& transform) const {
	Outline result = *this;
	for (Vector2f& point : result.points) {
		point = transform.apply(point);
	}
	return result;
}

Outline Outline::getEmboldened(float xStrength, float yStrength) const {
	Outline result = *this;
	if (this->points.empty() || (xStrength == 0.0f && yStrength == 0.0f)) {
		return result;
	}

	// Each contour's points, as index ranges: a contour starts at a move.
	std::vector<std::size_t> starts;
	std::size_t index = 0;
	for (const OutlineVerb verb : this->verbs) {
		if (verb == OutlineVerb::Move) {
			starts.push_back(index);
		}
		index += verb == OutlineVerb::Move || verb == OutlineVerb::Line ? 1 : (verb == OutlineVerb::Quad ? 2 : (verb == OutlineVerb::Cubic ? 3 : 0));
	}
	starts.push_back(this->points.size());

	// With y up, a clockwise shape (negative area, TrueType's convention) has
	// its filled side to the right of travel, so the outward normal of a
	// direction (x, y) is (-y, x); a counter-clockwise shape is the mirror.
	float area = 0.0f;
	for (std::size_t contour = 0; contour + 1 < starts.size(); contour++) {
		for (std::size_t i = starts[contour]; i < starts[contour + 1]; i++) {
			const std::size_t next = i + 1 < starts[contour + 1] ? i + 1 : starts[contour];
			area += this->points[i].cross(this->points[next]);
		}
	}
	const float side = area < 0.0f ? 1.0f : -1.0f;
	auto outward = [side](Vector2f direction) {
		return Vector2f{-direction.y * side, direction.x * side};
	};

	for (std::size_t contour = 0; contour + 1 < starts.size(); contour++) {
		const std::size_t first = starts[contour];
		const std::size_t count = starts[contour + 1] - first;
		if (count < 2) {
			continue;
		}
		for (std::size_t i = 0; i < count; i++) {
			const Vector2f point = this->points[first + i];
			// The nearest different points before and after, around the contour.
			std::size_t before = (i + count - 1) % count;
			while (before != i && this->points[first + before] == point) {
				before = (before + count - 1) % count;
			}
			std::size_t after = (i + 1) % count;
			while (after != i && this->points[first + after] == point) {
				after = (after + 1) % count;
			}
			if (before == i || after == i) {
				continue;
			}
			const Vector2f incoming = point - this->points[first + before];
			const Vector2f outgoing = this->points[first + after] - point;
			const Vector2f inNormal = outward(incoming * (1.0f / incoming.getLength()));
			const Vector2f outNormal = outward(outgoing * (1.0f / outgoing.getLength()));
			const Vector2f bisector = inNormal + outNormal;
			Vector2f shift = inNormal;
			const float length = bisector.getLength();
			if (length > 1e-3f) {
				const Vector2f direction = bisector * (1.0f / length);
				shift = direction * (1.0f / std::max(direction.dot(inNormal), 0.5f));
			}
			result.points[first + i] = point + Vector2f{shift.x * xStrength * 0.5f, shift.y * yStrength * 0.5f};
		}
	}
	return result;
}

Rectf Outline::getBounds() const {
	if (this->points.empty()) {
		return Rectf{};
	}

	Vector2f minimum = this->points.front();
	Vector2f maximum = this->points.front();
	auto include = [&minimum, &maximum](Vector2f point) {
		minimum.x = std::min(minimum.x, point.x);
		minimum.y = std::min(minimum.y, point.y);
		maximum.x = std::max(maximum.x, point.x);
		maximum.y = std::max(maximum.y, point.y);
	};

	// Curves can bulge past their end points: include the points where the
	// derivative of each coordinate is zero, if they lie inside the segment.
	auto includeQuadExtrema = [&include](Vector2f p0, Vector2f p1, Vector2f p2) {
		auto evaluate = [&](float t) {
			const float u = 1.0f - t;
			return p0 * (u * u) + p1 * (2.0f * u * t) + p2 * (t * t);
		};
		const Vector2f denominator = p0 - p1 * 2.0f + p2;
		if (denominator.x != 0.0f) {
			const float t = (p0.x - p1.x) / denominator.x;
			if (t > 0.0f && t < 1.0f) {
				include(evaluate(t));
			}
		}
		if (denominator.y != 0.0f) {
			const float t = (p0.y - p1.y) / denominator.y;
			if (t > 0.0f && t < 1.0f) {
				include(evaluate(t));
			}
		}
	};

	auto includeCubicExtrema = [&include](Vector2f p0, Vector2f p1, Vector2f p2, Vector2f p3) {
		auto evaluate = [&](float t) {
			const float u = 1.0f - t;
			return p0 * (u * u * u) + p1 * (3.0f * u * u * t) + p2 * (3.0f * u * t * t) + p3 * (t * t * t);
		};
		auto solveAxis = [&](float v0, float v1, float v2, float v3) {
			// The derivative divided by 3 is qa t^2 + qb t + qc.
			const float first = v1 - v0;
			const float second = v2 - v1;
			const float third = v3 - v2;
			const float qa = first - 2.0f * second + third;
			const float qb = 2.0f * (second - first);
			const float qc = first;
			auto consider = [&](float t) {
				if (t > 0.0f && t < 1.0f) {
					include(evaluate(t));
				}
			};
			if (std::abs(qa) < 1e-12f) {
				if (qb != 0.0f) {
					consider(-qc / qb);
				}
				return;
			}
			const float discriminant = qb * qb - 4.0f * qa * qc;
			if (discriminant < 0.0f) {
				return;
			}
			const float root = std::sqrt(discriminant);
			consider((-qb + root) / (2.0f * qa));
			consider((-qb - root) / (2.0f * qa));
		};
		solveAxis(p0.x, p1.x, p2.x, p3.x);
		solveAxis(p0.y, p1.y, p2.y, p3.y);
	};

	Vector2f current;
	std::size_t index = 0;
	for (const OutlineVerb verb : this->verbs) {
		switch (verb) {
		case OutlineVerb::Move:
		case OutlineVerb::Line:
			current = this->points[index];
			include(current);
			index += 1;
			break;
		case OutlineVerb::Quad:
			include(this->points[index + 1]);
			includeQuadExtrema(current, this->points[index], this->points[index + 1]);
			current = this->points[index + 1];
			index += 2;
			break;
		case OutlineVerb::Cubic:
			include(this->points[index + 2]);
			includeCubicExtrema(current, this->points[index], this->points[index + 1], this->points[index + 2]);
			current = this->points[index + 2];
			index += 3;
			break;
		case OutlineVerb::Close:
			break;
		}
	}

	return Rectf{minimum.x, minimum.y, maximum.x - minimum.x, maximum.y - minimum.y};
}

Polylines Outline::flatten(float tolerance) const {
	constexpr float smallestTolerance = 1e-4f;
	constexpr float maximumSegments = 1024.0f;
	tolerance = std::max(tolerance, smallestTolerance);

	Polylines result;
	auto startContour = [&result](Vector2f point) {
		// A contour needs at least two points to enclose anything.
		if (!result.contourStarts.empty() && result.points.size() - result.contourStarts.back() < 2) {
			result.points.resize(result.contourStarts.back());
			result.contourStarts.pop_back();
		}
		result.contourStarts.push_back(static_cast<std::uint32_t>(result.points.size()));
		result.points.push_back(point);
	};
	auto segmentCount = [tolerance, maximumSegments](float curvature, float factor) {
		// Uniform steps in t keep the chord within the tolerance of the curve:
		// the error of n steps is at most factor * curvature / n^2.
		const float count = std::ceil(std::sqrt(factor * curvature / tolerance));
		return static_cast<std::int32_t>(std::clamp(count, 1.0f, maximumSegments));
	};

	Vector2f current;
	bool inContour = false;
	std::size_t index = 0;
	for (const OutlineVerb verb : this->verbs) {
		switch (verb) {
		case OutlineVerb::Move:
			current = this->points[index];
			startContour(current);
			inContour = true;
			index += 1;
			break;
		case OutlineVerb::Line:
			if (!inContour) {
				startContour(current);
				inContour = true;
			}
			current = this->points[index];
			result.points.push_back(current);
			index += 1;
			break;
		case OutlineVerb::Quad: {
			if (!inContour) {
				startContour(current);
				inContour = true;
			}
			const Vector2f p0 = current;
			const Vector2f p1 = this->points[index];
			const Vector2f p2 = this->points[index + 1];
			const std::int32_t steps = segmentCount((p0 - p1 * 2.0f + p2).getLength(), 0.25f);
			for (std::int32_t step = 1; step <= steps; step++) {
				const float t = static_cast<float>(step) / static_cast<float>(steps);
				const float u = 1.0f - t;
				result.points.push_back(p0 * (u * u) + p1 * (2.0f * u * t) + p2 * (t * t));
			}
			current = p2;
			index += 2;
			break;
		}
		case OutlineVerb::Cubic: {
			if (!inContour) {
				startContour(current);
				inContour = true;
			}
			const Vector2f p0 = current;
			const Vector2f p1 = this->points[index];
			const Vector2f p2 = this->points[index + 1];
			const Vector2f p3 = this->points[index + 2];
			const float curvature = std::max((p0 - p1 * 2.0f + p2).getLength(), (p1 - p2 * 2.0f + p3).getLength());
			const std::int32_t steps = segmentCount(curvature, 0.75f);
			for (std::int32_t step = 1; step <= steps; step++) {
				const float t = static_cast<float>(step) / static_cast<float>(steps);
				const float u = 1.0f - t;
				result.points.push_back(p0 * (u * u * u) + p1 * (3.0f * u * u * t) + p2 * (3.0f * u * t * t) + p3 * (t * t * t));
			}
			current = p3;
			index += 3;
			break;
		}
		case OutlineVerb::Close:
			inContour = false;
			break;
		}
	}

	if (!result.contourStarts.empty() && result.points.size() - result.contourStarts.back() < 2) {
		result.points.resize(result.contourStarts.back());
		result.contourStarts.pop_back();
	}
	return result;
}

bool Outline::isEmpty() const {
	return this->points.empty();
}

std::string Outline::toSvgPath() const {
	std::string path;
	auto addToken = [&path](std::string_view token) {
		if (!path.empty()) {
			path += ' ';
		}
		path += token;
	};
	auto addNumber = [&addToken](float value) {
		addToken(value == 0.0f ? std::string("0") : std::format("{}", value));
	};
	auto addPoint = [&addNumber](Vector2f point) {
		addNumber(point.x);
		addNumber(point.y);
	};

	std::size_t index = 0;
	for (const OutlineVerb verb : this->verbs) {
		switch (verb) {
		case OutlineVerb::Move:
			addToken("M");
			addPoint(this->points[index]);
			index += 1;
			break;
		case OutlineVerb::Line:
			addToken("L");
			addPoint(this->points[index]);
			index += 1;
			break;
		case OutlineVerb::Quad:
			addToken("Q");
			addPoint(this->points[index]);
			addPoint(this->points[index + 1]);
			index += 2;
			break;
		case OutlineVerb::Cubic:
			addToken("C");
			addPoint(this->points[index]);
			addPoint(this->points[index + 1]);
			addPoint(this->points[index + 2]);
			index += 3;
			break;
		case OutlineVerb::Close:
			addToken("Z");
			break;
		}
	}
	return path;
}

const std::vector<OutlineVerb>& Outline::getVerbs() const {
	return this->verbs;
}

const std::vector<Vector2f>& Outline::getPoints() const {
	return this->points;
}

}
