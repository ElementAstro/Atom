/**
 * @file fits_wcs.hpp
 * @brief FITS World Coordinate System (WCS) support
 *
 * This file provides comprehensive WCS support following the FITS WCS
 * standard for coordinate transformations between pixel and world
 * coordinates.
 *
 * Supported projections include TAN, SIN, AIT, CAR, and others commonly
 * used in astronomical imaging.
 *
 * @copyright Copyright (C) 2023-2025
 */

#ifndef ATOM_IMAGE_FITS_WCS_HPP
#define ATOM_IMAGE_FITS_WCS_HPP

#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fits_header.hpp"

namespace atom::image::fits {

/**
 * @enum ProjectionType
 * @brief Supported WCS projection types
 */
enum class ProjectionType {
    // Zenithal/Azimuthal projections
    TAN,  ///< Gnomonic (tangent plane)
    SIN,  ///< Orthographic (sine)
    ARC,  ///< Zenithal equidistant
    ZPN,  ///< Zenithal polynomial
    ZEA,  ///< Zenithal equal-area
    AIR,  ///< Airy

    // Cylindrical projections
    CAR,  ///< Plate carrée (Cartesian)
    MER,  ///< Mercator
    CEA,  ///< Cylindrical equal-area
    CYP,  ///< Cylindrical perspective

    // Pseudo-cylindrical projections
    SFL,  ///< Sanson-Flamsteed
    PAR,  ///< Parabolic
    MOL,  ///< Mollweide
    AIT,  ///< Hammer-Aitoff

    // Conic projections
    COP,  ///< Conic perspective
    COE,  ///< Conic equal-area
    COD,  ///< Conic equidistant
    COO,  ///< Conic orthomorphic

    // Polyconic and pseudo-conic
    BON,  ///< Bonne
    PCO,  ///< Polyconic

    // Quad-cube projections
    TSC,  ///< Tangential spherical cube
    CSC,  ///< COBE spherical cube
    QSC,  ///< Quadrilateralized spherical cube

    // Special
    HPX,  ///< HEALPix

    LINEAR,  ///< Linear (no projection)
    UNKNOWN  ///< Unknown projection
};

/**
 * @struct WCSCoordinate
 * @brief A coordinate pair in world or pixel space
 */
struct WCSCoordinate {
    double x = 0.0;  ///< X coordinate (RA or pixel X)
    double y = 0.0;  ///< Y coordinate (Dec or pixel Y)

    WCSCoordinate() = default;
    WCSCoordinate(double x_, double y_) : x(x_), y(y_) {}
};

/**
 * @struct CelestialCoord
 * @brief Celestial coordinates (RA/Dec or Galactic l/b)
 */
struct CelestialCoord {
    double lon = 0.0;  ///< Longitude (RA or l) in degrees
    double lat = 0.0;  ///< Latitude (Dec or b) in degrees

    CelestialCoord() = default;
    CelestialCoord(double lon_, double lat_) : lon(lon_), lat(lat_) {}

    /**
     * @brief Get RA in hours
     */
    [[nodiscard]] double raHours() const noexcept { return lon / 15.0; }

    /**
     * @brief Format as HMS/DMS string
     */
    [[nodiscard]] std::string toHMSDMS() const;

    /**
     * @brief Format as decimal degrees string
     */
    [[nodiscard]] std::string toDecimalDegrees() const;
};

/**
 * @struct WCSParams
 * @brief WCS transformation parameters
 */
struct WCSParams {
    // Reference point
    std::array<double, 2> crpix = {0.0, 0.0};  ///< Reference pixel (CRPIX)
    std::array<double, 2> crval = {0.0, 0.0};  ///< Reference value (CRVAL)

    // Pixel scale (alternative representations)
    std::array<double, 2> cdelt = {1.0, 1.0};  ///< Pixel scale (CDELT)
    std::array<std::array<double, 2>, 2> cd = {
        {{1.0, 0.0}, {0.0, 1.0}}};  ///< CD matrix
    std::array<std::array<double, 2>, 2> pc = {
        {{1.0, 0.0}, {0.0, 1.0}}};  ///< PC matrix

    double crota2 = 0.0;     ///< Rotation angle (CROTA2)
    double lonpole = 180.0;  ///< Native longitude of pole
    double latpole = 0.0;    ///< Native latitude of pole

    // Axis types
    std::array<std::string, 2> ctype = {"", ""};  ///< Axis types (CTYPE)
    std::array<std::string, 2> cunit = {"", ""};  ///< Axis units (CUNIT)

    // Projection parameters (for ZPN, etc.)
    std::vector<double> pv;  ///< Projection parameters (PVi_j)

    ProjectionType projection = ProjectionType::TAN;
    bool useCDMatrix = false;  ///< Use CD matrix instead of CDELT/PC
    bool hasRotation = false;  ///< Has CROTA2 rotation
    int naxis = 2;             ///< Number of axes
};

/**
 * @class WCSTransform
 * @brief World Coordinate System transformation
 *
 * Provides bidirectional transformation between pixel coordinates
 * and world (celestial) coordinates following the FITS WCS standard.
 */
class WCSTransform {
public:
    /**
     * @brief Default constructor (identity transform)
     */
    WCSTransform();

    /**
     * @brief Construct from WCS parameters
     * @param params WCS parameters
     */
    explicit WCSTransform(const WCSParams& params);

    /**
     * @brief Construct from FITS header
     * @param header FITS header containing WCS keywords
     */
    explicit WCSTransform(const FITSHeader& header);

    /**
     * @brief Construct from keyword getter function
     * @param getKeyword Function to get keyword values
     */
    explicit WCSTransform(
        std::function<std::optional<std::string>(const std::string&)>
            getKeyword);

    /**
     * @brief Virtual destructor
     */
    virtual ~WCSTransform() = default;

    /**
     * @brief Transform pixel coordinates to world coordinates
     * @param pixX Pixel X coordinate (1-indexed)
     * @param pixY Pixel Y coordinate (1-indexed)
     * @return World coordinates (RA, Dec in degrees)
     */
    [[nodiscard]] CelestialCoord pixelToWorld(double pixX, double pixY) const;

    /**
     * @brief Transform pixel coordinates to world coordinates
     * @param pixel Pixel coordinate
     * @return World coordinates
     */
    [[nodiscard]] CelestialCoord pixelToWorld(const WCSCoordinate& pixel) const;

    /**
     * @brief Transform world coordinates to pixel coordinates
     * @param ra Right Ascension in degrees
     * @param dec Declination in degrees
     * @return Pixel coordinates (1-indexed)
     */
    [[nodiscard]] WCSCoordinate worldToPixel(double ra, double dec) const;

    /**
     * @brief Transform world coordinates to pixel coordinates
     * @param world World coordinates
     * @return Pixel coordinates
     */
    [[nodiscard]] WCSCoordinate worldToPixel(const CelestialCoord& world) const;

    /**
     * @brief Get the pixel scale in arcseconds per pixel
     * @return Pixel scale (x, y) in arcseconds/pixel
     */
    [[nodiscard]] std::pair<double, double> getPixelScale() const;

    /**
     * @brief Get the image rotation angle
     * @return Rotation angle in degrees (N through E)
     */
    [[nodiscard]] double getRotationAngle() const;

    /**
     * @brief Get the image center in world coordinates
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @return Center world coordinates
     */
    [[nodiscard]] CelestialCoord getImageCenter(int width, int height) const;

    /**
     * @brief Get the field of view
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @return Field of view (width, height) in degrees
     */
    [[nodiscard]] std::pair<double, double> getFieldOfView(int width,
                                                           int height) const;

    /**
     * @brief Check if WCS is valid
     * @return True if valid
     */
    [[nodiscard]] bool isValid() const noexcept { return valid_; }

    /**
     * @brief Get projection type
     * @return Projection type
     */
    [[nodiscard]] ProjectionType getProjection() const noexcept {
        return params_.projection;
    }

    /**
     * @brief Get WCS parameters
     * @return WCS parameters structure
     */
    [[nodiscard]] const WCSParams& getParams() const noexcept {
        return params_;
    }

    /**
     * @brief Set WCS parameters
     * @param params New parameters
     */
    void setParams(const WCSParams& params);

    /**
     * @brief Create header keywords from current WCS
     * @return Vector of keyword-value pairs
     */
    [[nodiscard]] std::vector<std::pair<std::string, std::string>>
    toHeaderKeywords() const;

    /**
     * @brief Calculate angular separation between two sky positions
     * @param coord1 First coordinate
     * @param coord2 Second coordinate
     * @return Angular separation in degrees
     */
    [[nodiscard]] static double angularSeparation(const CelestialCoord& coord1,
                                                  const CelestialCoord& coord2);

    /**
     * @brief Calculate position angle from coord1 to coord2
     * @param coord1 From coordinate
     * @param coord2 To coordinate
     * @return Position angle in degrees (N through E)
     */
    [[nodiscard]] static double positionAngle(const CelestialCoord& coord1,
                                              const CelestialCoord& coord2);

protected:
    /**
     * @brief Parse WCS keywords from header
     */
    void parseHeader(
        std::function<std::optional<std::string>(const std::string&)>
            getKeyword);

    /**
     * @brief Parse projection type from CTYPE
     */
    [[nodiscard]] ProjectionType parseProjection(
        const std::string& ctype) const;

    /**
     * @brief Initialize transformation matrices
     */
    void initializeMatrices();

    /**
     * @brief Apply forward spherical projection
     */
    [[nodiscard]] std::pair<double, double> projectForward(double phi,
                                                           double theta) const;

    /**
     * @brief Apply inverse spherical projection
     */
    [[nodiscard]] std::pair<double, double> projectInverse(double x,
                                                           double y) const;

    /**
     * @brief Spherical to native coordinates
     */
    [[nodiscard]] std::pair<double, double> sphericalToNative(
        double alpha, double delta) const;

    /**
     * @brief Native to spherical coordinates
     */
    [[nodiscard]] std::pair<double, double> nativeToSpherical(
        double phi, double theta) const;

private:
    WCSParams params_;
    bool valid_ = false;

    // Precomputed transformation matrix (inverse of CD or CDELT*PC)
    std::array<std::array<double, 2>, 2> cdInverse_;
    double det_ = 1.0;

    // Trigonometric values for pole
    double sinLatpole_ = 0.0;
    double cosLatpole_ = 1.0;
    double sinDec0_ = 0.0;
    double cosDec0_ = 1.0;

    /**
     * @brief Compute inverse of 2x2 matrix
     */
    void computeInverse();

    /**
     * @brief Parse numeric keyword value
     */
    [[nodiscard]] std::optional<double> parseDouble(
        const std::optional<std::string>& value) const;
};

/**
 * @brief Parse projection type from CTYPE string
 * @param ctype CTYPE value (e.g., "RA---TAN")
 * @return Projection type
 */
[[nodiscard]] ProjectionType projectionFromCTYPE(const std::string& ctype);

/**
 * @brief Get projection name string
 * @param type Projection type
 * @return Projection name
 */
[[nodiscard]] std::string projectionToString(ProjectionType type);

// Pi constant for cross-platform compatibility
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Convert degrees to radians
 */
[[nodiscard]] constexpr double degToRad(double deg) noexcept {
    return deg * 3.14159265358979323846 / 180.0;
}

/**
 * @brief Convert radians to degrees
 */
[[nodiscard]] constexpr double radToDeg(double rad) noexcept {
    return rad * 180.0 / 3.14159265358979323846;
}

/**
 * @brief Normalize angle to [0, 360) degrees
 */
[[nodiscard]] double normalizeAngle360(double angle) noexcept;

/**
 * @brief Normalize angle to [-180, 180) degrees
 */
[[nodiscard]] double normalizeAngle180(double angle) noexcept;

}  // namespace atom::image::fits

#endif  // ATOM_IMAGE_FITS_WCS_HPP
