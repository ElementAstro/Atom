/**
 * @file fits_wcs.cpp
 * @brief Implementation of FITS World Coordinate System support
 *
 * @copyright Copyright (C) 2023-2025
 */

#include "fits_wcs.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace atom::image::fits {

namespace {

constexpr double PI = M_PI;
constexpr double TWOPI = 2.0 * M_PI;
constexpr double HALFPI = M_PI / 2.0;
constexpr double D2R = M_PI / 180.0;
constexpr double R2D = 180.0 / M_PI;

}  // anonymous namespace

// CelestialCoord methods

std::string CelestialCoord::toHMSDMS() const {
    // Convert RA to HMS
    double raHrs = lon / 15.0;
    if (raHrs < 0)
        raHrs += 24.0;

    int raH = static_cast<int>(raHrs);
    double raMinFrac = (raHrs - raH) * 60.0;
    int raM = static_cast<int>(raMinFrac);
    double raS = (raMinFrac - raM) * 60.0;

    // Convert Dec to DMS
    double absDec = std::abs(lat);
    int decD = static_cast<int>(absDec);
    double decMinFrac = (absDec - decD) * 60.0;
    int decM = static_cast<int>(decMinFrac);
    double decS = (decMinFrac - decM) * 60.0;
    char decSign = lat >= 0 ? '+' : '-';

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << raH << "h " << std::setw(2)
        << raM << "m " << std::fixed << std::setprecision(2) << std::setw(5)
        << raS << "s  " << decSign << std::setw(2) << decD << "° "
        << std::setw(2) << decM << "' " << std::setprecision(1) << std::setw(4)
        << decS << "\"";

    return oss.str();
}

std::string CelestialCoord::toDecimalDegrees() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << lon << "° "
        << (lat >= 0 ? "+" : "") << lat << "°";
    return oss.str();
}

// WCSTransform constructors

WCSTransform::WCSTransform() {
    initializeMatrices();
    valid_ = true;
}

WCSTransform::WCSTransform(const WCSParams& params) : params_(params) {
    initializeMatrices();
    valid_ = true;
}

WCSTransform::WCSTransform(const FITSHeader& header) {
    auto getKeyword =
        [&header](const std::string& key) -> std::optional<std::string> {
        try {
            return header.getKeywordValue(key);
        } catch (...) {
            return std::nullopt;
        }
    };

    parseHeader(getKeyword);
    initializeMatrices();
}

WCSTransform::WCSTransform(
    std::function<std::optional<std::string>(const std::string&)> getKeyword) {
    parseHeader(getKeyword);
    initializeMatrices();
}

void WCSTransform::parseHeader(
    std::function<std::optional<std::string>(const std::string&)> getKeyword) {
    valid_ = false;

    // Parse CTYPE
    auto ctype1 = getKeyword("CTYPE1");
    auto ctype2 = getKeyword("CTYPE2");

    if (ctype1) {
        params_.ctype[0] = *ctype1;
        // Remove quotes
        auto& ct = params_.ctype[0];
        while (!ct.empty() && (ct.front() == '\'' || ct.front() == ' ')) {
            ct.erase(0, 1);
        }
        while (!ct.empty() && (ct.back() == '\'' || ct.back() == ' ')) {
            ct.pop_back();
        }
        params_.projection = parseProjection(params_.ctype[0]);
    }

    if (ctype2) {
        params_.ctype[1] = *ctype2;
        auto& ct = params_.ctype[1];
        while (!ct.empty() && (ct.front() == '\'' || ct.front() == ' ')) {
            ct.erase(0, 1);
        }
        while (!ct.empty() && (ct.back() == '\'' || ct.back() == ' ')) {
            ct.pop_back();
        }
    }

    // Parse CRPIX
    auto crpix1 = parseDouble(getKeyword("CRPIX1"));
    auto crpix2 = parseDouble(getKeyword("CRPIX2"));

    if (crpix1)
        params_.crpix[0] = *crpix1;
    if (crpix2)
        params_.crpix[1] = *crpix2;

    // Parse CRVAL
    auto crval1 = parseDouble(getKeyword("CRVAL1"));
    auto crval2 = parseDouble(getKeyword("CRVAL2"));

    if (crval1)
        params_.crval[0] = *crval1;
    if (crval2)
        params_.crval[1] = *crval2;

    // Try CD matrix first
    auto cd1_1 = parseDouble(getKeyword("CD1_1"));
    auto cd1_2 = parseDouble(getKeyword("CD1_2"));
    auto cd2_1 = parseDouble(getKeyword("CD2_1"));
    auto cd2_2 = parseDouble(getKeyword("CD2_2"));

    if (cd1_1 && cd2_2) {
        params_.useCDMatrix = true;
        params_.cd[0][0] = *cd1_1;
        params_.cd[0][1] = cd1_2.value_or(0.0);
        params_.cd[1][0] = cd2_1.value_or(0.0);
        params_.cd[1][1] = *cd2_2;
    } else {
        // Try CDELT/CROTA or CDELT/PC
        auto cdelt1 = parseDouble(getKeyword("CDELT1"));
        auto cdelt2 = parseDouble(getKeyword("CDELT2"));

        if (cdelt1)
            params_.cdelt[0] = *cdelt1;
        if (cdelt2)
            params_.cdelt[1] = *cdelt2;

        // Check for CROTA2
        auto crota2 = parseDouble(getKeyword("CROTA2"));
        if (crota2) {
            params_.crota2 = *crota2;
            params_.hasRotation = true;
        }

        // Check for PC matrix
        auto pc1_1 = parseDouble(getKeyword("PC1_1"));
        auto pc1_2 = parseDouble(getKeyword("PC1_2"));
        auto pc2_1 = parseDouble(getKeyword("PC2_1"));
        auto pc2_2 = parseDouble(getKeyword("PC2_2"));

        if (pc1_1 || pc1_2 || pc2_1 || pc2_2) {
            params_.pc[0][0] = pc1_1.value_or(1.0);
            params_.pc[0][1] = pc1_2.value_or(0.0);
            params_.pc[1][0] = pc2_1.value_or(0.0);
            params_.pc[1][1] = pc2_2.value_or(1.0);
        }
    }

    // Parse LONPOLE and LATPOLE
    auto lonpole = parseDouble(getKeyword("LONPOLE"));
    auto latpole = parseDouble(getKeyword("LATPOLE"));

    if (lonpole)
        params_.lonpole = *lonpole;
    if (latpole)
        params_.latpole = *latpole;

    // Parse CUNIT
    auto cunit1 = getKeyword("CUNIT1");
    auto cunit2 = getKeyword("CUNIT2");

    if (cunit1)
        params_.cunit[0] = *cunit1;
    if (cunit2)
        params_.cunit[1] = *cunit2;

    // Parse PV parameters (for ZPN and other projections)
    for (int i = 1; i <= 2; ++i) {
        for (int j = 0; j <= 20; ++j) {
            std::string key =
                "PV" + std::to_string(i) + "_" + std::to_string(j);
            auto pv = parseDouble(getKeyword(key));
            if (pv) {
                while (params_.pv.size() <= static_cast<size_t>(j)) {
                    params_.pv.push_back(0.0);
                }
                params_.pv[j] = *pv;
            }
        }
    }

    // Validate
    if (crpix1 && crpix2 && crval1 && crval2) {
        valid_ = true;
    }
}

void WCSTransform::initializeMatrices() {
    // Build the effective CD matrix
    if (!params_.useCDMatrix) {
        if (params_.hasRotation) {
            // Use CDELT + CROTA2
            double cosRot = std::cos(params_.crota2 * D2R);
            double sinRot = std::sin(params_.crota2 * D2R);

            params_.cd[0][0] = params_.cdelt[0] * cosRot;
            params_.cd[0][1] = -params_.cdelt[1] * sinRot;
            params_.cd[1][0] = params_.cdelt[0] * sinRot;
            params_.cd[1][1] = params_.cdelt[1] * cosRot;
        } else {
            // Use CDELT * PC
            params_.cd[0][0] = params_.cdelt[0] * params_.pc[0][0];
            params_.cd[0][1] = params_.cdelt[0] * params_.pc[0][1];
            params_.cd[1][0] = params_.cdelt[1] * params_.pc[1][0];
            params_.cd[1][1] = params_.cdelt[1] * params_.pc[1][1];
        }
    }

    computeInverse();

    // Precompute trigonometric values
    sinLatpole_ = std::sin(params_.latpole * D2R);
    cosLatpole_ = std::cos(params_.latpole * D2R);
    sinDec0_ = std::sin(params_.crval[1] * D2R);
    cosDec0_ = std::cos(params_.crval[1] * D2R);
}

void WCSTransform::computeInverse() {
    // Compute determinant
    det_ = params_.cd[0][0] * params_.cd[1][1] -
           params_.cd[0][1] * params_.cd[1][0];

    if (std::abs(det_) < 1e-20) {
        det_ = 1.0;
        cdInverse_[0][0] = 1.0;
        cdInverse_[0][1] = 0.0;
        cdInverse_[1][0] = 0.0;
        cdInverse_[1][1] = 1.0;
        return;
    }

    // Inverse matrix
    cdInverse_[0][0] = params_.cd[1][1] / det_;
    cdInverse_[0][1] = -params_.cd[0][1] / det_;
    cdInverse_[1][0] = -params_.cd[1][0] / det_;
    cdInverse_[1][1] = params_.cd[0][0] / det_;
}

CelestialCoord WCSTransform::pixelToWorld(double pixX, double pixY) const {
    if (!valid_) {
        throw std::runtime_error("WCS is not valid");
    }

    // Step 1: Pixel to intermediate world coordinates
    double dx = pixX - params_.crpix[0];
    double dy = pixY - params_.crpix[1];

    double x = params_.cd[0][0] * dx + params_.cd[0][1] * dy;
    double y = params_.cd[1][0] * dx + params_.cd[1][1] * dy;

    // Step 2: Apply projection (inverse spherical projection)
    auto [phi, theta] = projectInverse(x, y);

    // Step 3: Native to celestial coordinates
    auto [alpha, delta] = nativeToSpherical(phi, theta);

    return CelestialCoord(normalizeAngle360(alpha), delta);
}

CelestialCoord WCSTransform::pixelToWorld(const WCSCoordinate& pixel) const {
    return pixelToWorld(pixel.x, pixel.y);
}

WCSCoordinate WCSTransform::worldToPixel(double ra, double dec) const {
    if (!valid_) {
        throw std::runtime_error("WCS is not valid");
    }

    // Step 1: Celestial to native coordinates
    auto [phi, theta] = sphericalToNative(ra, dec);

    // Step 2: Apply projection (forward spherical projection)
    auto [x, y] = projectForward(phi, theta);

    // Step 3: Intermediate world to pixel coordinates
    double dx = cdInverse_[0][0] * x + cdInverse_[0][1] * y;
    double dy = cdInverse_[1][0] * x + cdInverse_[1][1] * y;

    double pixX = dx + params_.crpix[0];
    double pixY = dy + params_.crpix[1];

    return WCSCoordinate(pixX, pixY);
}

WCSCoordinate WCSTransform::worldToPixel(const CelestialCoord& world) const {
    return worldToPixel(world.lon, world.lat);
}

std::pair<double, double> WCSTransform::projectForward(double phi,
                                                       double theta) const {
    double phiRad = phi * D2R;
    double thetaRad = theta * D2R;
    double sinTheta = std::sin(thetaRad);
    double cosTheta = std::cos(thetaRad);
    double sinPhi = std::sin(phiRad);
    double cosPhi = std::cos(phiRad);

    double x = 0.0, y = 0.0;

    switch (params_.projection) {
        case ProjectionType::TAN: {
            // Gnomonic (tangent plane)
            if (sinTheta <= 0) {
                throw std::runtime_error("Point not on visible hemisphere");
            }
            double r = R2D * cosTheta / sinTheta;
            x = r * sinPhi;
            y = -r * cosPhi;
            break;
        }

        case ProjectionType::SIN: {
            // Orthographic
            x = R2D * cosTheta * sinPhi;
            y = -R2D * cosTheta * cosPhi;
            break;
        }

        case ProjectionType::ARC: {
            // Zenithal equidistant
            double r = R2D * (HALFPI - thetaRad);
            x = r * sinPhi;
            y = -r * cosPhi;
            break;
        }

        case ProjectionType::ZEA: {
            // Zenithal equal-area
            double r = R2D * std::sqrt(2.0 * (1.0 - sinTheta));
            x = r * sinPhi;
            y = -r * cosPhi;
            break;
        }

        case ProjectionType::CAR: {
            // Plate carrée
            x = phi;
            y = theta;
            break;
        }

        case ProjectionType::MER: {
            // Mercator
            if (std::abs(thetaRad) >= HALFPI) {
                throw std::runtime_error("Point at pole");
            }
            x = phi;
            y = R2D * std::log(std::tan((HALFPI + thetaRad) / 2.0));
            break;
        }

        case ProjectionType::AIT: {
            // Hammer-Aitoff
            double gamma = std::sqrt(2.0 / (1.0 + cosTheta * cosPhi / 2.0));
            x = 2.0 * R2D * gamma * cosTheta * sinPhi / 2.0;
            y = R2D * gamma * sinTheta;
            break;
        }

        case ProjectionType::LINEAR:
        default:
            x = phi;
            y = theta;
            break;
    }

    return {x, y};
}

std::pair<double, double> WCSTransform::projectInverse(double x,
                                                       double y) const {
    double phi = 0.0, theta = 0.0;

    switch (params_.projection) {
        case ProjectionType::TAN: {
            // Gnomonic (tangent plane)
            double r = std::sqrt(x * x + y * y);
            if (r == 0.0) {
                phi = 0.0;
                theta = 90.0;
            } else {
                phi = std::atan2(x, -y) * R2D;
                theta = std::atan(R2D / r) * R2D;
            }
            break;
        }

        case ProjectionType::SIN: {
            // Orthographic
            double r = std::sqrt(x * x + y * y) * D2R;
            if (r > 1.0) {
                throw std::runtime_error("Point outside projection");
            }
            phi = std::atan2(x, -y) * R2D;
            theta = std::acos(r) * R2D;
            break;
        }

        case ProjectionType::ARC: {
            // Zenithal equidistant
            double r = std::sqrt(x * x + y * y);
            phi = std::atan2(x, -y) * R2D;
            theta = 90.0 - r;
            break;
        }

        case ProjectionType::ZEA: {
            // Zenithal equal-area
            double r = std::sqrt(x * x + y * y) * D2R;
            if (r > 2.0) {
                throw std::runtime_error("Point outside projection");
            }
            phi = std::atan2(x, -y) * R2D;
            theta = 90.0 - 2.0 * std::asin(r / 2.0) * R2D;
            break;
        }

        case ProjectionType::CAR: {
            // Plate carrée
            phi = x;
            theta = y;
            break;
        }

        case ProjectionType::MER: {
            // Mercator
            phi = x;
            theta = 2.0 * std::atan(std::exp(y * D2R)) * R2D - 90.0;
            break;
        }

        case ProjectionType::AIT: {
            // Hammer-Aitoff (approximate inverse)
            double z = std::sqrt(1.0 - (x * D2R / 4.0) * (x * D2R / 4.0) -
                                 (y * D2R / 2.0) * (y * D2R / 2.0));
            phi = 2.0 * std::atan2(z * x * D2R / 2.0, 2.0 * z * z - 1.0) * R2D;
            theta = std::asin(z * y * D2R) * R2D;
            break;
        }

        case ProjectionType::LINEAR:
        default:
            phi = x;
            theta = y;
            break;
    }

    return {phi, theta};
}

std::pair<double, double> WCSTransform::sphericalToNative(double alpha,
                                                          double delta) const {
    double alphaRad = alpha * D2R;
    double deltaRad = delta * D2R;
    double alpha0Rad = params_.crval[0] * D2R;
    double delta0Rad = params_.crval[1] * D2R;

    double sinDelta = std::sin(deltaRad);
    double cosDelta = std::cos(deltaRad);
    double sinDelta0 = std::sin(delta0Rad);
    double cosDelta0 = std::cos(delta0Rad);
    double dAlpha = alphaRad - alpha0Rad;
    double cosDAlpha = std::cos(dAlpha);
    double sinDAlpha = std::sin(dAlpha);

    // Native longitude
    double num = -cosDelta * sinDAlpha;
    double den = sinDelta * cosDelta0 - cosDelta * sinDelta0 * cosDAlpha;
    double phi = std::atan2(num, den) * R2D;

    // Adjust for LONPOLE
    phi = phi + params_.lonpole - 180.0;

    // Native latitude
    double theta =
        std::asin(sinDelta * sinDelta0 + cosDelta * cosDelta0 * cosDAlpha) *
        R2D;

    return {normalizeAngle180(phi), theta};
}

std::pair<double, double> WCSTransform::nativeToSpherical(double phi,
                                                          double theta) const {
    double phiRad = (phi - params_.lonpole + 180.0) * D2R;
    double thetaRad = theta * D2R;
    double delta0Rad = params_.crval[1] * D2R;

    double sinTheta = std::sin(thetaRad);
    double cosTheta = std::cos(thetaRad);
    double sinDelta0 = std::sin(delta0Rad);
    double cosDelta0 = std::cos(delta0Rad);
    double sinPhi = std::sin(phiRad);
    double cosPhi = std::cos(phiRad);

    // Celestial latitude (declination)
    double sinDelta = sinTheta * sinDelta0 - cosTheta * cosDelta0 * cosPhi;
    double delta = std::asin(sinDelta) * R2D;

    // Celestial longitude (right ascension)
    double num = -cosTheta * sinPhi;
    double den = sinTheta * cosDelta0 + cosTheta * sinDelta0 * cosPhi;
    double dAlpha = std::atan2(num, den) * R2D;
    double alpha = params_.crval[0] + dAlpha;

    return {alpha, delta};
}

std::pair<double, double> WCSTransform::getPixelScale() const {
    // Calculate pixel scale in arcseconds per pixel
    double scale1 = std::sqrt(params_.cd[0][0] * params_.cd[0][0] +
                              params_.cd[1][0] * params_.cd[1][0]) *
                    3600.0;
    double scale2 = std::sqrt(params_.cd[0][1] * params_.cd[0][1] +
                              params_.cd[1][1] * params_.cd[1][1]) *
                    3600.0;
    return {std::abs(scale1), std::abs(scale2)};
}

double WCSTransform::getRotationAngle() const {
    // Calculate rotation angle (position angle of Y axis)
    return std::atan2(params_.cd[0][1], params_.cd[1][1]) * R2D;
}

CelestialCoord WCSTransform::getImageCenter(int width, int height) const {
    return pixelToWorld(width / 2.0 + 0.5, height / 2.0 + 0.5);
}

std::pair<double, double> WCSTransform::getFieldOfView(int width,
                                                       int height) const {
    // Calculate corners
    auto corner1 = pixelToWorld(1, 1);
    auto corner2 = pixelToWorld(width, 1);
    auto corner3 = pixelToWorld(1, height);
    auto corner4 = pixelToWorld(width, height);

    // Calculate approximate FOV
    double fovX = angularSeparation(corner1, corner2);
    double fovY = angularSeparation(corner1, corner3);

    return {fovX, fovY};
}

void WCSTransform::setParams(const WCSParams& params) {
    params_ = params;
    initializeMatrices();
    valid_ = true;
}

std::vector<std::pair<std::string, std::string>>
WCSTransform::toHeaderKeywords() const {
    std::vector<std::pair<std::string, std::string>> keywords;

    keywords.emplace_back("CTYPE1", "'" + params_.ctype[0] + "'");
    keywords.emplace_back("CTYPE2", "'" + params_.ctype[1] + "'");
    keywords.emplace_back("CRPIX1", std::to_string(params_.crpix[0]));
    keywords.emplace_back("CRPIX2", std::to_string(params_.crpix[1]));
    keywords.emplace_back("CRVAL1", std::to_string(params_.crval[0]));
    keywords.emplace_back("CRVAL2", std::to_string(params_.crval[1]));

    if (params_.useCDMatrix) {
        keywords.emplace_back("CD1_1", std::to_string(params_.cd[0][0]));
        keywords.emplace_back("CD1_2", std::to_string(params_.cd[0][1]));
        keywords.emplace_back("CD2_1", std::to_string(params_.cd[1][0]));
        keywords.emplace_back("CD2_2", std::to_string(params_.cd[1][1]));
    } else {
        keywords.emplace_back("CDELT1", std::to_string(params_.cdelt[0]));
        keywords.emplace_back("CDELT2", std::to_string(params_.cdelt[1]));

        if (params_.hasRotation) {
            keywords.emplace_back("CROTA2", std::to_string(params_.crota2));
        }
    }

    if (!params_.cunit[0].empty()) {
        keywords.emplace_back("CUNIT1", "'" + params_.cunit[0] + "'");
    }
    if (!params_.cunit[1].empty()) {
        keywords.emplace_back("CUNIT2", "'" + params_.cunit[1] + "'");
    }

    return keywords;
}

double WCSTransform::angularSeparation(const CelestialCoord& coord1,
                                       const CelestialCoord& coord2) {
    double ra1 = coord1.lon * D2R;
    double dec1 = coord1.lat * D2R;
    double ra2 = coord2.lon * D2R;
    double dec2 = coord2.lat * D2R;

    double sinDec1 = std::sin(dec1);
    double cosDec1 = std::cos(dec1);
    double sinDec2 = std::sin(dec2);
    double cosDec2 = std::cos(dec2);
    double cosDRA = std::cos(ra2 - ra1);

    double sep = std::acos(sinDec1 * sinDec2 + cosDec1 * cosDec2 * cosDRA);

    return sep * R2D;
}

double WCSTransform::positionAngle(const CelestialCoord& coord1,
                                   const CelestialCoord& coord2) {
    double ra1 = coord1.lon * D2R;
    double dec1 = coord1.lat * D2R;
    double ra2 = coord2.lon * D2R;
    double dec2 = coord2.lat * D2R;

    double dRA = ra2 - ra1;
    double cosDec2 = std::cos(dec2);

    double num = std::sin(dRA) * cosDec2;
    double den = std::cos(dec1) * std::sin(dec2) -
                 std::sin(dec1) * cosDec2 * std::cos(dRA);

    double pa = std::atan2(num, den) * R2D;

    return normalizeAngle360(pa);
}

ProjectionType WCSTransform::parseProjection(const std::string& ctype) const {
    // Extract projection code from CTYPE (e.g., "RA---TAN" -> "TAN")
    size_t dashPos = ctype.find('-');
    if (dashPos == std::string::npos) {
        return ProjectionType::LINEAR;
    }

    // Find the projection code after the dashes
    std::string code;
    for (size_t i = dashPos; i < ctype.size(); ++i) {
        if (ctype[i] != '-' && std::isalpha(ctype[i])) {
            code = ctype.substr(i, 3);
            break;
        }
    }

    return projectionFromCTYPE(code);
}

std::optional<double> WCSTransform::parseDouble(
    const std::optional<std::string>& value) const {
    if (!value || value->empty()) {
        return std::nullopt;
    }

    std::string str = *value;

    // Remove quotes
    while (!str.empty() && (str.front() == '\'' || str.front() == ' ')) {
        str.erase(0, 1);
    }
    while (!str.empty() && (str.back() == '\'' || str.back() == ' ')) {
        str.pop_back();
    }

    // Remove comments
    size_t slashPos = str.find('/');
    if (slashPos != std::string::npos) {
        str = str.substr(0, slashPos);
    }

    // Handle 'D' exponent
    std::replace(str.begin(), str.end(), 'D', 'E');
    std::replace(str.begin(), str.end(), 'd', 'e');

    try {
        return std::stod(str);
    } catch (...) {
        return std::nullopt;
    }
}

// Utility functions

ProjectionType projectionFromCTYPE(const std::string& ctype) {
    std::string code = ctype;

    // Convert to uppercase
    std::transform(code.begin(), code.end(), code.begin(), ::toupper);

    // Remove any trailing spaces
    while (!code.empty() && code.back() == ' ') {
        code.pop_back();
    }

    if (code == "TAN")
        return ProjectionType::TAN;
    if (code == "SIN")
        return ProjectionType::SIN;
    if (code == "ARC")
        return ProjectionType::ARC;
    if (code == "ZPN")
        return ProjectionType::ZPN;
    if (code == "ZEA")
        return ProjectionType::ZEA;
    if (code == "AIR")
        return ProjectionType::AIR;
    if (code == "CAR")
        return ProjectionType::CAR;
    if (code == "MER")
        return ProjectionType::MER;
    if (code == "CEA")
        return ProjectionType::CEA;
    if (code == "CYP")
        return ProjectionType::CYP;
    if (code == "SFL")
        return ProjectionType::SFL;
    if (code == "PAR")
        return ProjectionType::PAR;
    if (code == "MOL")
        return ProjectionType::MOL;
    if (code == "AIT")
        return ProjectionType::AIT;
    if (code == "COP")
        return ProjectionType::COP;
    if (code == "COE")
        return ProjectionType::COE;
    if (code == "COD")
        return ProjectionType::COD;
    if (code == "COO")
        return ProjectionType::COO;
    if (code == "BON")
        return ProjectionType::BON;
    if (code == "PCO")
        return ProjectionType::PCO;
    if (code == "TSC")
        return ProjectionType::TSC;
    if (code == "CSC")
        return ProjectionType::CSC;
    if (code == "QSC")
        return ProjectionType::QSC;
    if (code == "HPX")
        return ProjectionType::HPX;

    return ProjectionType::UNKNOWN;
}

std::string projectionToString(ProjectionType type) {
    switch (type) {
        case ProjectionType::TAN:
            return "TAN (Gnomonic)";
        case ProjectionType::SIN:
            return "SIN (Orthographic)";
        case ProjectionType::ARC:
            return "ARC (Zenithal Equidistant)";
        case ProjectionType::ZPN:
            return "ZPN (Zenithal Polynomial)";
        case ProjectionType::ZEA:
            return "ZEA (Zenithal Equal-Area)";
        case ProjectionType::AIR:
            return "AIR (Airy)";
        case ProjectionType::CAR:
            return "CAR (Plate Carrée)";
        case ProjectionType::MER:
            return "MER (Mercator)";
        case ProjectionType::CEA:
            return "CEA (Cylindrical Equal-Area)";
        case ProjectionType::CYP:
            return "CYP (Cylindrical Perspective)";
        case ProjectionType::SFL:
            return "SFL (Sanson-Flamsteed)";
        case ProjectionType::PAR:
            return "PAR (Parabolic)";
        case ProjectionType::MOL:
            return "MOL (Mollweide)";
        case ProjectionType::AIT:
            return "AIT (Hammer-Aitoff)";
        case ProjectionType::COP:
            return "COP (Conic Perspective)";
        case ProjectionType::COE:
            return "COE (Conic Equal-Area)";
        case ProjectionType::COD:
            return "COD (Conic Equidistant)";
        case ProjectionType::COO:
            return "COO (Conic Orthomorphic)";
        case ProjectionType::BON:
            return "BON (Bonne)";
        case ProjectionType::PCO:
            return "PCO (Polyconic)";
        case ProjectionType::TSC:
            return "TSC (Tangential Spherical Cube)";
        case ProjectionType::CSC:
            return "CSC (COBE Spherical Cube)";
        case ProjectionType::QSC:
            return "QSC (Quadrilateralized Spherical Cube)";
        case ProjectionType::HPX:
            return "HPX (HEALPix)";
        case ProjectionType::LINEAR:
            return "LINEAR";
        case ProjectionType::UNKNOWN:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

double normalizeAngle360(double angle) noexcept {
    angle = std::fmod(angle, 360.0);
    if (angle < 0.0)
        angle += 360.0;
    return angle;
}

double normalizeAngle180(double angle) noexcept {
    angle = std::fmod(angle + 180.0, 360.0);
    if (angle < 0.0)
        angle += 360.0;
    return angle - 180.0;
}

}  // namespace atom::image::fits
