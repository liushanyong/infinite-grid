ACGE 
 AcGeAugPolyline3d  Represents a mathematical entity used to support different types of spline curves in 3D space. 
  
 AcGeBoundBlock2d  This class provides a two-dimensional bounding volume implementation. Bounding blocks may exist in two forms: as an ordinary coordinate-aligned bounding box and as a parallelogram.  
 AcGeBoundBlock3d  This class provides spatial dimensional bounding volume implementation. Bounding blocks may exist in two forms: as an ordinary coordinate-aligned bounding box and as a parallelepiped.  
 AcGeBoundedPlane  Represents a bounded planar parametric surface.  
 AcGeCircArc2d  Represents both full circles and circular arcs in 2D space.  
 AcGeCircArc3d  Represents both full circles and circular arcs in 3D space.  
 AcGeClipBoundary2d  This class defines a planar clipping object for clipping arbitrary 2D polygons or polylines to 2D convex open or closed polyline curves or rectangles. The implementation is similar to the Sutherland-Hodgman pipeline clipping algorithm.
Detailed edge information can optionally be generated to track the source of each clipped output segment. This information can be used to control edge display or to efficiently stitch together output from multiple clip boundaries (possibly from decomposing a non-convex clip boundary into multiple convex boundaries).
Information about the spatial relationship between the clipped polygon or polyline and the clip region is also generated.  
 AcGeCompositeCurve2d  Represents a composite curve, which is a single curve that consists of any number of component curves that are connected end-to-end. For instance, a composite curve could consist of four straight line segments that form a square. Each of the component curves must be bounded curves, which means that infinite lines and rays may not be used as component curves of a composite. A composite curve object stores a list of pointers to the component curves.
The start parameter of a composite is 0.0. The parameter value at the endpoint of the first component curve is the approximate length of... more  
 AcGeCompositeCurve3d  Represents a composite curve, which is a single curve that consists of any number of component curves that are connected end-to-end. For instance, a composite curve could consist of four straight line segments that form a square. Each of the component curves must be bounded curves, which means that infinite lines and rays may not be used as component curves of a composite. The component curves are not required to all lie in the same plane. A composite curve object stores a list of pointers to the component curves.
The start parameter of a composite is 0.0. The parameter value... more  
 AcGeCone  Represents a bounded right circular cone.  
 AcGeCubicSplineCurve2d  Represent an interpolation cubic spline in 2D space.  
 AcGeCubicSplineCurve3d  Represents an interpolation cubic spline in 3D space.  
 AcGeCurve2d  Abstract base class for all 2D curves. Any class that is derived from this class represents a 2D curve.  
 AcGeCurve3d  Abstract base class for all 3D curves. Any class that is derived from this class represents a 3D curve.  
 AcGeCurveBoundary  AcGeCurveBoundary is a class that is used to communicate boundary geometry that exists on a bounded surface. In the most general case, each instance of this class consists of four arrays: an array of 3D curve or 3D position pointers, an array of parameter space curve pointers, and two arrays of Booleans. One Boolean array indicates the logical orientation for each 3D curve, and the other indicates the logical orientation for each parameter space curve.
Collectively, these four arrays represent an array of oriented 3D space curves or 3D positions, and an array of oriented parameter space curves. There is... more  
 AcGeCurveCurveInt2d  Represents the intersection set of two curves. There are special case intersectWith() functions in the AcGeLinearEnt2d, AcGeCircArc2d, and AcGeEllipArc2d classes to find the points of intersection between two lines, a line and an arc, an arc and an arc, or a line and an ellipse. However, there is no general intersectWith() function to find the points of intersection between two general curves. This can only be done by constructing an object of this class.
Objects of this class are constructed by specifying the two curves that are to be intersected. It is also possible to specify a specific... more  
 AcGeCurveCurveInt3d  Represents the intersection set of two curves. There are special case intersectWith() functions in the AcGeLinearEnt3d, AcGeCircArc3d, and AcGeEllipArc3d classes to find the points of intersection between two lines, a line and an arc, an arc and an arc, or a line and an ellipse. However, there is no general intersectWith() function to find the points of intersection between two general curves. This can only be done by constructing an object of this class.
Objects of this class are constructed by specifying the two curves that are to be intersected. It is also possible to specify a specific... more  
 AcGeCurveSurfInt  This class holds data for intersections of a 3D curve and a surface.  
 AcGeCylinder  Represents a bounded cylinder surface.  
 AcGeDwgIO  This utility class performs DWG file input and output.
  
 AcGeDxfIO  This utility class performs DXF file input and output.
  
 AcGeEllipArc2d  Represents both full ellipses and elliptical arcs in 2D space.  
 AcGeEllipArc3d  Represents both full ellipses and elliptical arcs in 3D space.  
 AcGeEntity2d  Abstract base class for all point set classes. Any class that is derived from this class represents a set of points in 2D space.  
 AcGeEntity3d  Abstract base class for all point set classes. Any class that is derived from this class represents a set of points in 3D space.  
 AcGeExternalBoundedSurface  AcGeExternalBoundedSurface class is a representation of a bounded surface defined outside of the geometry library. A logical view of this class is as follows: An instance of AcGeExternalBoundedSurface consists of an instance of AcGeExternalSurface representing the unbounded surface geometry, together with a collection of instances of AcGeCurveBoundary, each representing a boundary on the unbounded surface.
It is important to realize that the representation is geometric. No topological information is represented in this class. This assertion is made explicit by deriving this class from AcGeSurface. A typical use of AcGeExternalBoundedSurface would be to represent an ACIS FACE (that is,... more  
 AcGeExternalCurve2d  AcGeExternalCurve2d class is a representation for a 2D curve geometry that has its definition external to GeLib. Since AcGeExternalCurve2d is a AcGeCurve2d, these external entities can be treated as any other GeLib 2D curves. Certain external curve points sets may have corresponding native representations in GeLib. In such cases, it is possible to obtain such GeLib representations. However, the curve definitions of native GeLib curves would differ from the definition of external entities. An example of such a difference is differing curve parameterizations. A typical example of a use of AcGeExternalCurve2d would be to represent a ShapeManager pcurve.... more  
 AcGeExternalCurve3d  AcGeExternalCurve3d class is a representation for a 3D curve geometry that has its definition external to gelib. Since AcGeExternalCurve3d is an AcGeCurve3d, these external entities can be treated as any other gelib 3D curves. Certain external curve points sets may have corresponding native representations in GeLib. In such cases, it is possible to obtain such GeLib representations. However, the curve definitions of native GeLib curves would differ from the definition of external entities. An example of such a difference is differing curve parameterizations. A typical example of a use of AcGeExternalCurve3d is to represent a ShapeManager curve.... more  
 AcGeExternalSurface  AcGeExternalSurface class is a representation for an unbounded (other than by the parametric bounds of the surface) surface geometry that has its definition external to GeLib. Since AcGeExternalSurface is a AcGeSurface, these external entities can be treated as any other GeLib surface. Certain external surface points sets may have corresponding native representations in GeLib. In such cases, it is possible to obtain such GeLib representations. However, the surface definitions of native GeLib surfaces differ from the definition of external entities. An example of such a difference is differing surface parameterizations. A typical use of AcGeExternalSurface is to represent a... more  
 AcGeFileIO  This utility class performs file input and output.
  
 AcGeFiler  AcGeFiler defines the abstract interface for serialization requirements of AcGe entities. An application that requires input/output of the definition of AcGe entities in a format defined by the application should provide an implementation of this class. An instance of this class is provided as an argument to the entity serialization functions, thus allowing the input/output format to be controlled by the client of gelib.
For example:
  
 AcGeInterval  Represents a finite or semi-infinite interval as the real axis whose bounds are defined with some given tolerance.
  
 AcGeKnotVector  Represents an ordered non-decreasing sequence of numbers used in several spline entities.  
 AcGeLibVersion  This class provides functionality for managing versions of GeLib.
  
 AcGeLine2d  Represents an unbounded line in 2D space.
  
 AcGeLine3d  Represents an unbounded line in 3D space.
  
 AcGeLinearEnt2d  Abstract base class for all 2D line classes.  
 AcGeLinearEnt3d  Abstract base class for all 3D line classes.  
 AcGeLineSeg2d  Represents a bounded line segment in 2D space.  
 AcGeLineSeg3d  Represents a bounded line segment in 3D space.  
 AcGeMatrix2d  Class AcGeMatrix2d represents an affine transformation of 3D space, including translation. Each matrix M has the form:
  
 AcGeMatrix3d  Class AcGeMatrix3d represents an affine transformation of 3D space, including translation. Each matrix M has the form:
  
 AcGeNurbCurve2d  Represents non-uniform rational B-spline in 2D space.  
 AcGeNurbCurve3d  Represents non-uniform rational B-spline in 3D space.  
 AcGeNurbSurface  Represents generic NURB parametric surface.
  
 AcGeOffsetCurve2d  Represents a curve that is an exact offset of another curve (base curve). An object of this class stores a pointer to the base curve of which it is an offset. This means that any modification of the base curve also modifies the offset curve. Since this curve is an exact offset, it may be self-intersecting even though the base curve is not self-intersecting (a non-self-intersecting offset curve can be created by calling AcGeCurve2d::getTrimmedOffset.) 
The positive direction of offset at each point of the base curve is perpendicular to the tangent vector at that point. So if the tangent... more  
 AcGeOffsetCurve3d  Represents a curve that is an exact offset of another curve (base curve). An object of this class stores a pointer to the base curve of which it is an offset. This means that any modification of the base curve also modifies the offset curve. Since this curve is an exact offset, it may be self-intersecting even though the base curve is not self-intersecting (a non-self-intersecting offset curve can be created by calling AcGeCurve3d::getTrimmedOffset()). 
The base curve must be a planar curve. A normal vector must be specified when constructing an AcGeOffsetCurve3d object. This vector must be perpendicular to... more  
 AcGeOffsetSurface  Represents an offset object.
  
 AcGePlanarEnt  The class AcGePlanarEnt is an abstract base class for all planar surfaces. Any planar surface S is parameterized the following way: parameter point with coordinates u,v maps to the modeling space point S(u,v) by the formula.
  
 AcGePlane  Class AcGePlane represents a parameterized infinite plane in 3D space.
  
 AcGePoint2d  AcGePoint2d represents a point in 2-dimensional space.  
 AcGePoint3d  AcGePoint3d represents a point in 3D space.  
 AcGePointEnt2d  Abstract base class for 2D point classes.
  
 AcGePointEnt3d  Abstract base class for 3D point classes.
  
 AcGePointOnCurve2d  Represents a point on a 2D parametric curve. This class encapsulates all of the geometric information of a point on a 2D curve, including its parameter value, 2D coordinates, and derivatives.  
 AcGePointOnCurve3d  Represents a point on a 3D parametric curve. This class encapsulates all of the geometric information of a point on a 3D curve including its parameter value, 3D coordinates, and derivatives.  
 AcGePointOnSurface  Represents a point on a surface. This class encapsulates all of the geometric information of a point on a surface, including its parameter value, 3D coordinates, derivatives, and normal vector.  
 AcGePolyline2d  Represents a piecewise linear spline entity in 2D space.
  
 AcGePolyline3d  Represents a piecewise linear spline entity in 3D space.
  
 AcGePosition2d  Represents a point in 2D space.
  
 AcGePosition3d  Represents a point in 3D space.
  
 AcGeRay2d  Represents a half-bounded line in 2D space. An AcGeRay2d object either has a start point but no endpoint, or has an endpoint but no start point.
  
 AcGeRay3d  Represents a half-bounded line in 3D space. An AcGeRay3 object either has a start point but no endpoint, or has an endpoint but no start point.
  
 AcGeScale2d  The class AcGeScale2d is called "scale vector 2D." This class is used to represent scaling transformations in 2D space.
Contract: The scale vector must have no zero components (with tolerances 1.0e-10).
The data members of the class AcGeScale2d are two doubles, which are called "scale factors."  
 AcGeScale3d  The class AcGeScale3d is called "scale vector 3D." This class is used to represent scaling transformations in 3D space.
Contract: The scale vector must have no zero components (with tolerances 1.0e-10).
The data members of the class AcGeScale3d are three doubles, which are called "scale factors."  
 AcGeSphere  Represents a spherical surface.
  
 AcGeSplineEnt2d  Represents different spline entities in 2D space.
  
 AcGeSplineEnt3d  Represents different spline entities in 3D space.
  
 AcGeSurface  AcGeSurface class is an abstract base class for all other parametric surfaces.
  
 AcGeSurfSurfInt  This class holds data for intersections of two surfaces.
  
 AcGeTol  This is an instantiable class that is by default initialized to the default tolerances. Subsequently, the tolerances within it can be customized to suit a specific need. For example, an instance of this class may be specialized for use during surface intersection.
Class AcGeTol keeps two values, equalPoint and equalVector, which are used in evaluation according to the following rules:

Two points, p1 and p2, are equal if 
  
 AcGeTorus  Represents toroidal segment.
  
 AcGeVector2d  Class AcGeVector2d represents a vector in 2D space. It can be viewed as a structure consisting of two doubles.  
 AcGeVector3d  Class AcGeVector3d represents a vector in 3D space. It can be viewed as a structure consisting of 3 doubles.  

Types
Type  Description  
AcGeIntArray  AcGeIntArray is a template class that uses the AcArray class template. Please see AcArray for a complete description of the AcArray operators and methods.
  
AcGePoint2dArray  AcGePoint2dArray is a template class that uses the AcArray class template. Please see AcArray for a complete description of the AcArray operators and methods.
  
AcGePoint3dArray  AcGePoint3dArray is a template class that uses the AcArray class template. Please see AcArray for a complete description of the AcArray operators and methods.
  
AcGeVector2dArray  AcGeVector2dArray is a template class that uses the AcArray class template. Please see AcArray for a complete description of the AcArray operators and methods.
  
AcGeVector3dArray  AcGeVector3dArray is a template class that uses the AcArray class template. Please see AcArray for a complete description of the AcArray operators and methods.
  
AcGeVoidPointerArray  AcGeVoidPointerArray is a template class that uses the AcArray class template. Please see AcArray for a complete description of the AcArray operators and methods.
 
ACGI

 AcGiColorRGB  Color class for storing the color with a red, green, and blue component in double-precision format. 
  
 AcGiColorRGBA  Color class for storing the color with a red, green, and blue components, with alpha shading, in double-precision format. 
  
 AcGiCommonDraw  AcGiCommonDraw abstracts out the commonality between AcGiViewportDraw and AcGiWorldDraw. This allows graphics routines to be coded in terms of AcGiCommonDraw that will work in either case. To maintain their original signatures, AcGiWorldDraw and AcGiViewportDraw now derive from AcGiCommonDraw.  
 AcGiContext  The context can be thought of as the root object of the regeneration. A single instance of an AcGiContext-derived class serves as a point of reference during a given regen. It is accessible via both AcGiWorldDraw and AcGiViewportDraw and hence is always available to the client.
It is guaranteed that the AcGiViewportDraw passed in to viewportDraw() will return the same instance of AcGiContext as that returned by the instance of AcGiWorldDraw passed into the preceding call to worldDraw().
  
 AcGiContext2  The context can be thought of as the root object of the regeneration. A single instance of an AcGiContext-derived class serves as a point of reference during a given regen. It is accessible via both AcGiWorldDraw and AcGiViewportDraw and hence is always available to the client.
It is guaranteed that the AcGiViewportDraw passed in to viewportDraw() will return the same instance of AcGiContext as that returned by the instance of AcGiWorldDraw passed into the preceding call to worldDraw().  
 AcGiContextualColors  This class returns color information for objects that can have different colors in different display contexts.
  
 AcGiContextualColors2  Extended contextual color interface allowing for web mesh color traits.
  
 AcGiDistantLightTraits  This class represents a distant light source, whose light is emitted in parallel rays, with an infinite distance between the light source and the model. 
A light passing it's properties to this class should not rely on any default values.
  
 AcGiDrawable    
 AcGiDrawableEvent  AcGiDrawableEvent manages a set of AcGiDrawableReactors and sends notifications to them when certain events occur. Access is through a singleton instance of this class, acgiDrawableEvent. Clients of AcGiDrawableEvent fall into three categories: 
Any client interested in making use of AcGiDrawableReactor must add and remove their reactor through addReactor() and removeReactor(). 
AcGiDrawable-derived clients may use this class to notify graphics systems of modifications and erasures of their graphics. 
Graphics systems may use this class to notify interested listeners (e.g. AcGiDrawStream) of regen begin/end events. 
  
 AcGiDrawableOverrule  AcGiDrawableOverrule overrules a subset of draw related operations that the AcGiDrawable class specifies. It is intended as a base class for clients who want to alter some or all behavior of a given AcGiDrawable-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcGiDrawableReactor  AcGiDrawableReactor provides notifications of key AcGiDrawable-related events including begin/end regen, modification, and erasure. Clients who derive from this class will receive these events after registering their reactor with AcGiDrawableEvent.  
 AcGiDrawableTraits  This class allows a drawable to express its graphical traits to the graphics system. AcGiDrawableTraits exposes additional traits that are intended to be consistent for the whole drawable. This is a super set of those traits exposed in AcGiSubEntityTraits, which can be changed during the elaboration of the object. To express this relationship, AcGiDrawableTraits derives from AcGiSubEntityTraits.
The AcGiDrawableTraits interface is only available to an object during the setAttributes call made before worldDraw(). Only very specialized drawables will make use of this extension to AcGiSubEntityTraits.
  
 AcGiDrawStream  This class extends AcGi to support stream draw functionality, which is derived from AcGiDrawable.  
 AcGiEdgeData  This class is a container for the optional data that can be specified for a set of edges in a mesh or shell graphics primitive. An object of this class is meant to be passed as an argument to the mesh or shell methods of the AcGiWorldGeometry and AcGiViewportGeometry classes.  
 AcGiFaceData  This class is a container for the optional data that can be specified for a set of faces in a mesh or shell graphics primitive. An object of this class is passed as an argument to the mesh and shell methods of the AcGiGeometry classes.  
 AcGiFill  The base class of fill definition classes. This class also represents a solid fill.  
 AcGiGdiDrawObject  Derive from this abstract class to implement behavior related to the ownerDraw() API.  
 AcGiGenericTexture  A generic collection of parameters that represents a procedural texture. Derives from AcGiProceduralTexture. 
  
 AcGiGeometry  AcGiGeometry abstracts out the commonality between AcGiViewportGeometry and AcGiWorldGeometry. To maintain their original signatures, AcGiWorldGeometry and AcGiViewportGeometry now derive from AcGiGeometry.  
 AcGiGlyph  This class is used to provide the graphics primitives for drawing a glyph during snap operations.  
 AcGiGradientBackgroundTraits  Represents a gradient background to the underlying display system. An instance of this class is populated by AcDbGradientBackground through its SetAttributes method.
  
 AcGiGradientFill  Defines a gradient fill.  
 AcGiGraphicsKernel  The Graphics Kernel is an encapulation of the underlying graphics system. Each kernel has its own set of capabilities, and AutoCAD will pick the appropriate kernel to use based on the requested capabilities.  
 AcGiGroundPlaneBackgroundTraits  Represents a ground plane background to the underlying display system.
  
 AcGiHatchPattern  Defines the hatch pattern to be filled into the edge loops.  
 AcGiHatchPatternDefinition  Defines a line of the hatch pattern.  
 AcGiIBLBackgroundTraits  This class enables IBL background definitions to be elaborated to AcGi implementations.  
 AcGiImageBackgroundTraits  Represents an image background to the underlying display system. An instance of this class is populated by AcDbImageBackground through its SetAttributes method.
  
 AcGiImageBGRA32  Image class for storing the color with a red, green, and blue components, with alpha shading, in int-precision format.  
 AcGiImageFileTexture  An AcGiImageTexture derived class representing a file based image texture.
  
 AcGiImageTexture  An AcGiMaterialTexture derived class representing an image based texture. This class is abstract.
  
 AcGiKernelDescriptor  The kernel descriptor contains a set of capabilities that describe a graphics kernel. This class can be used to request a particular kernel, or to report the capabilities of a kernel.  
 AcGiLightAttenuation  Container class for light attenuation properties.
  
 AcGiLightTraits  Parent class for interfaces passed to AcDbLight objects in the implementation of setAttributes().The light first down-casts the traits class to the specific type, specified earlier in the addLight() call. It then sets specific light properties. 
This class derives from AcGiDrawableTraits, as that is what is passed into AcGiDrawable::setAttributes().
Lights on frozen layers will not be processed; their light will not illuminate the model.
  
 AcGiLineAttributes  AcGiLineAttributes is used to define line attribute overrides for DB entities, for viewports, and for plot style sheets.  
 AcGiLineAttrUtils  Utility functions to convert between different line attributes tables.  
 AcGiLinetypeEngine  This class elaborates a polyline with a linetype applied to it. The elaboration is performed through the AutoCAD graphics interface (AcGi). 
  
 AcGiMapper  The AcGiMapper class defines mappers. Mappers determine how an AcGiMaterialMap is positioned, sized, and rotated when it is mapped to an object surface. Mappers are described by type, tiling, automatic transform, and transform properties.
  
 AcGiMarbleTexture  An AcGiProceduralTexture derived class representing a "marble" procedural texture.
  
 AcGiMaterialColor  This class defines material component colors by color method and color value.
  
 AcGiMaterialMap  The AcGiMaterialMap class defines material component maps. Maps are described by dimension, source, format, and mapping properties.
  
 AcGiMaterialTexture  Base class for all material textures. This class is abstract.
  
 AcGiMaterialTraits  The AcGiMaterialTraits class allows material definitions to be expanded to AcGi implementations.
  
 AcGiMentalRayRenderSettingsTraits  Represents mental ray specific render settings to the underlying display system.  
 AcGiNonEntityTraits  The AcGiNonEntityTraits class implements empty AcGiDrawableTraits and AcGiSubEntityTraits interfaces to allow non-entity drawables, such as materials, to elaborate their unique properties through AcGiDrawableTraits. 
This class is not intended for direct use. Instead, it should be used as a base class for non-entity drawable traits.
  
 AcGiParameter  This class will be base class of all AcGi parameter classes, it provides the copy-on-write functionality with API/IMP mode.  
 AcGiPhotographicExposureParameters  Container class for all photographic exposure parameters.  
 AcGiPixelBGRA32  This is a color class for storing the color with a red, green, and blue components, with alpha shading, in an int-precision format.  
 AcGiPointLightTraits  This class represents a point light source, whose light is emitted in all directions.
  
 AcGiPolyline  Helper class for storing the parameters used in AcGiGeometry::polyline().  
 AcGiProceduralTexture  This is an abstract class, serving as the base for various procedural material textures.
  
 AcGiRapidRTRenderSettingsTraits  Abstract interface class that controls all the rendering settings.  
 AcGiRenderEnvironmentTraits  Represents render environment (fog and environment map) to the underlying display system.
  
 AcGiRenderSettingsTraits  Represents render settings to the underlying display system.  
 AcGiShadowParameters  This is a container class for all shadow related light properties.  
 AcGiSkyBackgroundTraits  This class enables sky background definitions to be elaborated to AcGi implementations.
  
 AcGiSkyParameters  Container class for all sky-related sun properties.
  
 AcGiSolidBackgroundTraits  Represents a solid background to the underlying display system. An instance of class is populated by AcDbSolidBackground through its SetAttributes method.
  
 AcGiSpotLightTraits  This class represents a spotlight source. Spotlights emit lights in a specific direction, and have an associated "light cone."
  
 AcGiStandardLightTraits  This intermediate class represents a standard, or non-photometric light. It defines properties common to all such lights. 
A light passing it's properties to this class should not rely on any default values.
  
 AcGiStyleAttributes  This class acts as an interface to AcGiStyleAttributes.
  
 AcGiSubEntityTraits  This class is used to control the current values of the color, layer, linetype, fill type, and graphics system marker attributes for graphics primitives.
Attribute settings are used for all graphics primitives drawn until the setting changed via one of the "set" functions in this class or the end of the current worldDraw() or viewportDraw() execution.
For instance, you can set the color attribute to blue and all of the subsequent geometry drawn during the current worldDraw() or viewportDraw() execution appears blue until you set the current color to something else.
  
 AcGiTextEngine  This class handles measurement and tessellation of AutoCAD Graphics Interface (AcGi) text entities. AcGi provides the graphics interface for both view-independent and view-dependent elaborations of AcDb entities.
  
 AcGiTextStyle  This class is used to set the display attributes of text. It is passed as an argument to one of the AcGiGeometry::text() member functions.  
 AcGiToneOperatorParameters  Container class for all tone parameters.
  
 AcGiTransientManager  Abstract base of transient manager. Developers can manage transients through this interface, including addition, deletion, and updating operations.  
 AcGiVariant  This class is used as the basis for the generic data storage used for procedural material definitions.  
 AcGiVertexData  This class is a container for the optional data that can be specified for a set of vertices for meshes and shells. An object of this class is meant to be passed in as an argument to AcGiGeometry::mesh() and AcGiGeometry::shell().  
 AcGiViewport  This class is used to pass back information to the user about the viewing characteristics of the current viewport. These characteristics are useful for generating an optimal graphics representation of the user's object, given the specific viewing configuration of the viewport.

For an explanation of coordinate systems, see the ObjectARX Developer’s Guide.  
 AcGiViewportDraw  The AcGiViewportDraw class is a container class that provides the ability to generate different geometry for different viewports. During entity regeneration, an entity's viewportDraw() method is only called if the entity's worldDraw() method returns Adesk::kFalse.
The default constructor creates new AcGiViewportGeometry, AcGiSubEntityTraits, and AcGiViewport objects, all of which are accessible via functions in the AcGiViewportDraw object.
An object of class AcGiViewportDraw is passed into the AcDbEntity::viewportDraw() function.
The AcGiViewportDraw class provides functions that return references to objects of other key AcGi classes. From AcGiViewportDraw, you can obtain objects of the following classes:
  
 AcGiViewportGeometry  This class is responsible for inserting geometry into the graphics cache for later display. An object of this class is part of every AcGiViewportDraw object which is passed into an entity's viewportDraw() method. Geometry generated with the methods of this class is displayed only in the viewport the viewportDraw method was called for. If non-viewport-specific graphics are desired, then worldDraw() should be used instead of viewportDraw().
This class also exposes the current net block insert transform of the entity.
Unless otherwise specified, model coordinates are applied to all geometry in this class. For an explanation of coordinate systems, see... more  
 AcGiViewportTraits  Viewports, on both Model and Layout tabs, now recieve a call to their AcDbViewport::setAttributes() method before AcDbViewport::worldDraw(), similar to entities. This gives viewports the opportunity to pass per-viewport properties to the display and rendering systems. 
A concrete instance of this interface class is passed to both AcDbViewport::setAttributes() and AcDbViewportTableRecord::setAttributes(). Set calls on this interface should then be made by the viewport object.
This class derives from AcGiDrawableTraits.
  
 AcGiVisualStyle  This class defines a Visual Style - a collection of visual properties that can be applied to graphical entities.  
 AcGiVisualStyleTraits  This class defines the visual style traits. Derived from AcGiDrawableTraits, this class provides the interfaces to implement for setting and returning the visual style via drawable traits.  
 AcGiWebLightTraits  Extended point light interface allowing for photometric web traits.
  
 AcGiWoodTexture  An AcGiProceduralTexture derived class representing a "wood" procedural texture.
  
 AcGiWorldDraw  The AcGiWorldDraw class provides functionality that has the ability to generate geometry that doesn't change for different viewports. The geometry may look different in different viewports (due to different zoom factors, viewpoints, etc.), but only one set of geometry is generated, which is then adjusted by AutoCAD for all the different viewports. For generating different geometry in different viewports, see AcGiViewportDraw.
The default constructor creates new AcGiWorldGeometry and AcGiSubEntityTraits objects, both of which are accessible via methods in the AcGiWorldDraw object.
An object of AcGiWorldDraw is passed into the AcDbEntity::worldDraw() function.
The AcGiWorldDraw class provides functions that return references... more  
 AcGiWorldGeometry  This class is responsible for inserting geometry into the graphics cache for later display. An object of this class is part of every AcGiWorldDraw object which is passed into an entity's worldDraw() method when graphics generation is required for the entity. Geometry generated with the methods of this class is used in all viewports. If viewport-specific graphics are desired, then viewportDraw() must be used instead of worldDraw().
This class also exposes the current net block insert transform of the entity and allows the user to set the object's extents to some value.
Model coordinates are applied to all geometry... more  

ACDB

 Ac3PointAngleConstraint  This class represents a angle constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between 3 constrained points.  
 AcAngleConstraint  This class represents a angle constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two constrained lines.  
 AcAutoConstrainEvaluationCallback  The client needs to implement this callback to provide constraint priorities and priority override for auto-constraint evaluation.  
 AcCenterPointConstraint  This class represents a Concentric constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between a point and a circle, arc or ellipse.  
 AcColinearConstraint  This class represents a Colinear (coincident) constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two constrained lines.  
 AcCompositeConstraint  This class represents a composite constraint node in the owning AcDbAssoc2dConstraintGroup. It is the base class for every kind of composite constraint. The owned constraints are all applied on(connected with) the same constrained geometries.  
 AcConcentricConstraint  This class represents a Concentric constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two circles, arcs or ellipses.  
 AcConstrained2PointsConstructionLine  This class represents a constrained unbounded construction line node in the owning AcDbAssoc2dConstraintGroup. Internally it is made coincident to two AcConstrainedPoint objects.  
 AcConstrainedArc  This class represents a constrained arc node in the owning AcDbAssoc2dConstraintGroup.  
 AcConstrainedBoundedEllipse  This class represents a constrained ellipse arc node in the owning AcDbAssoc2dConstraintGroup.  
 AcConstrainedBoundedLine  This class represents a constrained bounded line node in the owning AcDbAssoc2dConstraintGroup.  
 AcConstrainedCircle  This class represents a constrained circle node in the owning AcDbAssoc2dConstraintGroup.  
 AcConstrainedConstructionLine  This class represents a constrained unbounded construction line node in the owning AcDbAssoc2dConstraintGroup. It does not take any initial value, and its position is decided by the constraints applied on it.  
 AcConstrainedCurve  This class represents a constrained curve node in the owning AcDbAssoc2dConstraintGroup.  
 AcConstrainedDatumLine  This class represents a constrained datum unbounded line node in the owning AcDbAssoc2dConstraintGroup. It is read-only and for reference as datum line, e.g., X datum line or Y datum line.  
 AcConstrainedEllipse  This class represents a constrained ellipse node in the owning AcDbAssoc2dConstraintGroup.  
 AcConstrainedGeometry  This class represents a constrained geometry node in the owning AcDbAssoc2dConstraintGroup. It is the base class for every type of supported geometry.  
 AcConstrainedImplicitPoint  This class represents a constrained implicit point node in the owning AcDbAssoc2dConstraintGroup. A constrained implicit point is always associated with a constrained curve; it may be the start point, end point, center point or define point (only valid for spline control point now) of the curve.
A constrained implicit point does not hold a reference to a AcDbAssocGeomDependency object.  
 AcConstrainedLine  This class represents a constrained unbounded line node in the owning AcDbAssoc2dConstraintGroup.  
 AcConstrainedPoint  This class represents a constrained point node in the owning AcDbAssoc2dConstraintGroup. Usually It holds a reference to a AcDbAssocGeomDependency object which may be the insertion point of a block reference or a grip point of a custom entity.  
 AcConstrainedRigidSet  This class represents a constrained rigid set node in the owning AcDbAssoc2dConstraintGroup.  
 AcConstrainedSpline  This class represents a constrained NURBS spline node in the owning AcDbAssoc2dConstraintGroup.  
 AcConstraintGroupNode  This class represents a node owned by a AcDbAssoc2dConstraintGroup. Usually a node is connected with a number of other nodes and the connection between any two nodes is not directed.
An AcConstraintGroupNode object should always be created and deleted through higher level APIs and the memory pointed to by it is held internally and should never be released by the client directly.  
 AcDb2dPolyline  The AcDb2dPolyline class represents the 2D polyline entity within AutoCAD.  
 AcDb2dVertex  The AcDb2dVertex class represents the vertices in 2D polylines.
AcDb2dVertex is the only ObjectARX API entity that still passes its position in OCS. This is because the Z coordinate is kept in its owning AcDb2dPolyline for historical purposes. If you are not working in two dimensions, it may be better to use an AcDb3dPolyline, or an AcDbSpline.  
 AcDb2LineAngularDimension  The AcDb2LineAngularDimension class represents the angular dimension defined by two lines (as opposed to three points) within AutoCAD.  
 AcDb3dPolyline  The AcDb3dPolyline class represents the 3D polyline entity within AutoCAD.  
 AcDb3dPolylineVertex  The AcDb3dPolylineVertex class represents the vertices within 3D polylines in AutoCAD.  
 AcDb3dProfile  This class is used to represent a 3d profile that can be used as input to functions such as createExtrudedSurface(), createRevolvedSurface(), and createSweptSurface(). An object of this class can represent an entity such as a curve or a region, or it can represent an edge or a set of edges that form a connected chain.   
 AcDb3dSolid  Objects of this class represent 3D solids in AutoCAD.
An AcDb3dSolid entity is a container and interface for a ShapeManager object that is the actual geometrical representation of the solid.
This class provides various methods that are used to create solid primitives and to combine them to form a new single solid, much the same way solid objects are created using the AutoCAD command set.
This class provides no provisions for directly manipulating the edges, vertices, or faces of the ShapeManager object that is the actual solid representation.  
 AcDb3PointAngularDimension  The AcDb3PointAngularDimension class represents the angular dimension defined by three points (as opposed to two lines) within AutoCAD.  
 AcDbAbstractViewTable  AcDbAbstractViewTable is the base class for the two symbol table classes, AcDbViewTable and AcDbViewportTable.
  
 AcDbAbstractViewTableIterator  This class is for iterating through the records in an AcDbAbstractViewTable.
The only substantial difference from the base class AcDbSymbolTableIterator is the use of AcDbAbstractViewTableRecord-specific signatures for the member functions getRecord() and seek().  
 AcDbAbstractViewTableRecord  This class is the base class for the AcDbViewTableRecord and AcDbViewportTableRecord classes.  
 AcDbActionsToEvaluateCallback  This callback is used by AcDbAssocAction::getDependentActionsToEvaluate() to inform the caller about other actions, dependencies or arbitrary objects that should also be evaluated because they depend on the given action that is to be evaluated.  
 AcDbAlignedDimension  The AcDbAlignedDimension class is used to represent the dimension type that dimensions the distance between two points located anywhere in space. The dimension's normal vector must be perpendicular to the line between the two points. The two selected points are also used as the definition points for the start of the two dimension extension lines.  
 AcDbAlignment  This class allows applications to provide an alignment point and vector for a custom entity.
It is currently called only when alignment-grip editing an AcDbBlockReference.
  
 AcDbAnnotationScale  This class represents a single annotation scale defined in a drawing in the form of an object context. Objects of this class are obtained from the AcDbAnnotationScale context collection.  
 AcDbAnnotationScaleReactor  This class handles notifications sent when an annotation scale changes. Applications can derive from this class and implement their own version of the function annotationScaleChanged(), which will then get called during notification.

There are four cases where a notification is sent:

When AcDbImpViewport::setAnnotationScaleId has been called, a bool is set if the annotation scale of the viewport changes. If the bool is true, notification is sent during the AcDbImpViewport::subClose(). 
When a new viewport is created and AcDbImpViewport::setAnnotationScaleId() is called for the first time. 
When the sysvar routine, AcDbHeader::setCannoscale has been called with a annotative scale change. 
When AcDbImpViewport::subClose()... more 
 
 AcDbAnnotativeObjectPE  This protocol extension interface must be implemented for any object which supports annotation scaling. 
  
 AcDbAppEntityGrips  For internal use only.
  
 AcDbAppSystemVariables  This class serves as the handler for application-wide system variables. Prior to AutoCAD 2000, these system variables could be accessed through the database header. They are now stored in the registry.
  
 AcDbArc  The AcDbArc
class represents the arc entity within AutoCAD.  
 AcDbArcDimension  This class represents an arc length dimension.
  
 AcDbArrayGripAppData  AcDbArrayGripAppData represents grip specific data for an associative array. Objects of this class are attached with AcDbGripData as appdata by AcDbAssocArrayActionBody and its derived classes.  
 AcDbAssoc2dConstraintCallback  Client code can register callback(s) derived from AcDbAssoc2dConstraintCallback that are then called when events are triggered within an AcDbAssoc2dConstraintGroup.  
 AcDbAssoc2dConstraintGroup  The AcDbAssoc2dConstraintGroup class represents a set of geometric constraint relations between subentities of geometric entities. All the geometries must lie in a single plane.
The AcDbAssoc2dConstraintGroup object owns the constraints that are derived from the AcGeomConstraint class, and "proxies" of the constrained geometries, derived from the AcConstrainedGeometry class. AcConstrainedGeometries often refer to subentities of AutoCAD entities and the AcDbAssoc2dConstraintGroup owns AcDbAssocGeomDepedencies on these entities. It also owns AcDbAssocValueDependencies on the AcDbAssocVariables that are the values of dimensional constraints (represented by classes derived from AcExplicitConstraint).
If the dimensional constraint has a graphical representation, such as in the form of an AcDbDimension... more  
 AcDbAssocAction  Any object that wants to exhibit associative behavior, i.e. be automatically evaluated by the associative mechanism when objects it depends on change, needs to be derived from an AcDbAssocActionBody class (see comments at the AcDbAssocActionBody class) and owned by a parent AcDbAssocAction object. The action body is fully owned by its parent action, such as when the parent action is erased, it also erases the owned action body.
The AcDbAssocAction class itself does not have virtual methods that could be overridden, therefore it is not possible for external developers to derive directly from the AcDbAssocAction class and override its methods.... more  
 AcDbAssocActionBody  Abstract base class for deriving custom action body classes.
An object of a class derived from the AcDbAssocActionBody class is always owned by a parent AcDbAssocAction object. The AcDbAssocAction object has an AcDbHardOwnershipId on it and the AcDbAssocActionBody::ownerId() of the object returns the AcDbObjectId of its parent AcDbAssocAction object. Therefore a custom action object (in the logical sense of the word "object") is always represented by a pair of physical AcDbObjects: 
The parent action object of the AcDbAssocAction class (or, less commonly, of a derived class). 
The action body object of a custom class derived from the AcDbAssocActionBody abstract base... more 
 
 AcDbAssocActionParam  Abstract base class for all derived AcDbAssocXxxActionParam classes. Action parameters are owned by AcDbAssocParamBasedActionBody, either directly or via an AcDbAssocCompoundActionParam hierarchy.
Action parameters provide a uniform and high-level way for custom action bodies derived from AcDbAsscoParamBasedActionBody to keep and access their data. Instead of each custom action body implementing its own code to keep its data and devise its own representation of the data, the action body can keep all of its data in the form of action parameters. The action body then does not need to take care of the data serialization and cloning, because it is performed automatically... more  
 AcDbAssocArrayActionBody  AcDbAssocArrayActionBody is an associative action that can be evaluated to manage or position array items. Generally, this class manages assoc dependencies on various objects related to an associative array entity.
An array entity is generally represented as a block reference entity, which references an array block table record (array BTR). An array BTR contains a list of entities to represent items in the array. By default, this class represents an array item as an instance of AcDbBlocReference referencing a source block table record (source BTR) positioned at the item transform.
Derived classes may override array items and may choose to... more  
 AcDbAssocArrayCommonParameters  AcDbAssocArrayCommonParameters provides an interface for manipulating array parameters that are common to Rectangular, Path and Polar Arrays. Its derived classes implement logic for providing array item transforms as well as grip manipulations.  
 AcDbAssocArrayItem  AcDbAssocArrayItem represents transformation and other meta data of an item in an array. This class provides an interface for obtaining item transforms, it's locator (spatial index), relative transforms, etc.  
 AcDbAssocArrayModifyActionBody  AcDbAssocArrayModifyActionBody is an associative action which modifies or overrides items of an associative array. In general, this class and its derived classes manage item specific modifications on an associative array. In particular, this class is responsible for replacing some items with a set of source objects.
This class may be derived to support any modification action specific to array items.  
 AcDbAssocArrayParameters  AcDbAssocArrayParameters provides an interface for manipulating array parameters. Its derived classes implement logic for providing array item transforms as well as grip manipulation. AcDbAssocArrayActionBody owns the object of this class and uses its methods to evaluate the entire array. This class can also be used by a UI module to access parameters or provide jigging effects. This class provides two kinds of parameters, value parameters and geometry parameters.  
 AcDbAssocArrayPathParameters  AcDbAssocArrayPathParameters provides an interface for manipulating path array parameters.  
 AcDbAssocArrayPolarParameters  AcDbAssocArrayPolarParameters provides an interface for manipulating polar array parameters.  
 AcDbAssocArrayRectangularParameters  AcDbAssocArrayRectangularParameters provides an interface for manipulating rectangular array parameters.  
 AcDbAssocAsmBodyActionParam  Keeps "persistent" reference to an entity that has an ASM body. Alternatively, it may keep the ASM geometry directly instead of referencing to its owner AcDbEntity.   
 AcDbAssocBlendSurfaceActionBody  Action that sets the contents of AcDbSurface entity based on the surface blend operation. If this action is erased, the surface it is attached to will become a lofted surface and all the information stored in this action will get lost. The action can be fully associative or semi-associative. If fully associative, the resulting surface will always automatically regenerate when any input data is changed. If non-associative, when input surfaces are detached from the resulting surface, this action will be erased and the resulting surface will not regenerate and become a generic lofted surface.  
 AcDbAssocCompoundActionParam  Action parameter that owns other AcDbAssocActionParameters, allowing the representation of hierarchical structures of action parameters.  
 AcDbAssocDependency  AcDbAssocDependency represents information about which AcDbAssocAction objects depend on or modify which AcDbObject. This is how associativity in the drawing is represented. Dependencies are owned by actions and are attached to the objects as persistent reactors. When the depended-on object changes, it notifies the dependency via the persistent reactor mechanism. The dependency checks whether the change is relevant, such as when the dependency is on an endpoint of a line and the color of the line changed, or the other endpoint changed, and if yes, it changes its status indicating that it needs to be evaluated, and also changes the... more  
 AcDbAssocDependencyBody  
Abstract base class for deriving custom dependency body classes.
An object of a class derived from the AcDbAssocDependencyBody class is always owned by a parent AcDbAssocDependency object. The AcDbAssocDependency object has an AcDbHardOwnershipId on it and the AcDbAssocDependencyBody::ownerId() of the object returns the AcDbObjectId of its parent AcDbAssocDependency object.
Therefore a custom dependency object (in the logical sense of the word "object") is always represented by a pair of physical AcDbObjects: 
The parent dependency object of the AcDbAssocDependency class (or possibly, but less commonly, of a derived class). 
The dependency body object of a custom class derived from the 
AcDbAssocDependencyBody... more  
 AcDbAssocDependencyNotificationDisabler  Allows to disable change notifications to AcDbAssocDependencies.  
 AcDbAssocDependencyPE  AcRx protocol extension base class that allows objects and entities to control whether they allow AcDbAssocDependencies to be attached to them. When there is no protocol extension for the object's class, it means that the object allows both read and write-type dependencies.
Other AcDbAssocDependency-related protocol may later be added.  
 AcDbAssocDimDependencyBody  Abstract base class for deriving custom dependency body classes that implement functionality of custom dependencies.
An object of a class derived from the AcDbAssocDependencyBody class is always owned by a parent AcDbAssocDependency object. The AssocDependency object has an AcDbHardOwnershipId on it, and the AcDbAssocDependencyBody::ownerId() of the object returns the AcDbObjectId of its parent AcDbAssocDependency object.
Therefore a custom dependency object (in the logical sense of the word "object") is always represented by a pair of physical AcDbObjects:
The parent dependency object of the AcDbAssocDependency class (or possibly, but less commonly, of a derived class).
The dependency body object of a... more  
 AcDbAssocDimDependencyBodyBase  AcDbAssocDimDependencyBodyBase ties together the following objects that define a dimensional constraint:
AcDbAssocVariable Keeps dimensional constraint name and expression. AcDbAssoc2dConstraintGroup Keeps AcExplicitConstraint. AcDbEntity (such as AcDbDimension) Graphical representation of the dimensional constraint.
AcDbAssocDimDependencyBodyBase class represents a dependency of an AcExplicitConstraint owned by an AcDbAssoc2dConstraintGroup, on an AcDbEntity that is the graphical representation of the dimensional constraint. AcDbAssocDimDependencyBodyBase is an abstract base class, there are concrete derived classes (such as AcDbAssocDimDependencyBody) that deal with concrete entity types (such as AcDbDimension) that are used as graphical representations of dimensional constraints.
The AcDbAssocDimDependencyBodyBase does all the work of managing the graphical entity, receiving notifications... more  
 AcDbAssocEdgeActionParam  Action parameter that keeps reference to an edge, i.e. to a subentity whose geometry is a simple AcDbCurve3d.
It uses AcDbAssocGeomDependency to define a persistent reference to an edge subentity, such as to a segment of a polyline or to an edge of an AcDbSurface or of an AcDb3dSolid. Alternatively, it may keep AcDbAssocDependency to the whole entity that itself is just a simple edge, such as AcDbLine, AcDbCircle or AcDbArc. It may also directly keep the curve geometry in the action parameter in the form of an AcGeCurve3d*, instead of referencing some other AcDbEntity.
If the edge is part... more  
 AcDbAssocEdgeChamferActionBody  AcDbAssocEdgeChamferActionBody is an action body that controls the edge chamfer applied to a surface with the set of input data of the action. The "input data" for this action are a list of edges, chamfer distances, and the base face from which the chamfer base distance is measured and the surface to which the input edges belongs.  
 AcDbAssocEdgeFilletActionBody  AcDbAssocEdgeFilletActionBody is an action body that controls edge fillet applied to a surface with the set of input data of the action. The "input data" for this action are list of edges, fillet radius and the surface to which the input edges belongs.   
 AcDbAssocEdgePersSubentId  Used to identify edge subentities by indices of its two end vertices, or generally, by two arbitrary index identifiers.  
 AcDbAssocEvaluationCallback  A mechanism for AcDbAssocActions to notify about the progress of evaluation, to report success or failure, notify about which objects the action is going to use and modify, to allow the evaluation to be cancelled by the client, etc.   
 AcDbAssocExtendSurfaceActionBody  Action that sets the contents of the AcDbSurface created by edge extension; this action can work for both the extend modes: 
Append mode and it is a creation action; a new surface is created by extending given edges of the input surface. The surface will update itself when input surface is modified. In this mode, the action can be fully associative or semi-associative. if fully associative, the resulting surface will always automatically regenerate when any input data is changed. If semi-associative, when input surfaces are detached from the resulting surface; this action will be erased and the resulting surface will... more 
 
 AcDbAssocExtrudedSurfaceActionBody  Action that sets the contents of AcDbExtrudedSurface entity based on the input data of the action if the surface is associative. With this action, the extruded surface will always automatically regenerate when any input data is changed.   
 AcDbAssocFaceActionParam  This action parameter maintains a reference to a face. It uses a AcDbAssocGeomDependency to define a persistent reference to a face subentity of an AcDbEntity. Alternatively, it may directly maintain the face geometry in the action parameter in the form of an ASM body, instead of referencing some other AcDbEntity.  
 AcDbAssocFilletSurfaceActionBody  Action that sets the contents of AcDbSurface entity created by surface fillet operation. The action can be fully associative or semi-associative. if fully associative, the resulting surface will always automatically regenerate when any input data is changed. If semi-associative, when input surfaces are detached from the resulting surface, this action will be erased and the resulting surface will not regenerate and become a generic surface.  
 AcDbAssocGeomDependency  This class represents a dependency on a subentity (face/edge/vertex) of a geometric entity. It keeps an AcDbAssocPersSubentId that persistently identifies the subentity and provides a new protocol to set the referenced subentity and get/set the subentity geometry.
This class may also optionally cache the geometry of the referenced subentity that then allows the filtering-out of irrelevant change notifications that do no affect the referenced subentity. The caching is controlled by the setCachingSubentityGeometry() method.
By default the dependency does not cache the geometry of the referenced subentity. The isRelevantChange() predicate uses the base class implementation which calls AcDbAssocAction::isRelevantDependencyChange() of the owning... more  
 AcDbAssocIndexPersSubentId  Concrete derived AcDbAssocPersSubentId class that keeps a simple unsigned integer index that can be interpreted in any way the client (i.e. a concrete AcDbAssocPersSubentIdPE protocol extension) wants to.
This class is used as it is but also as base class for AcDbAssocAsmBasedEntityPersSubentId derived class and maybe more derived classes in the future.  
 AcDbAssocLoftedSurfaceActionBody  Action that sets the contents of AcDbLoftedSurface entity based on the input data of the action.   
 AcDbAssocManager  The global class that manages the associative framework. There is one instance of the manager per database.  
 AcDbAssocNetwork  The AcDbAssocNetwork class keeps a network of AcDbAssocActions. Note that it is derived from AcDbAssocAction, therefore it also behaves as an individual AcDbAssocAction. It means a whole network can depend on other objects via its AcDbAssocDependencies or be owned by a higher-level AcDbAssocNetwork, allowing the creation of hierarchical associative structures.
The AcDbAssocDependencies between AcDbAssocActions in the AcDbAssocNetwork define how the actions are tied together, how they depend on objects and on each other. For example, if one action modifies (writes) an object and another action uses (reads) the object, then the second action depends on the first one and the... more  
 AcDbAssocNetworkIterator  A simple class for iterating over all AcDbAssocActions of an AcDbAssocNetwork.  
 AcDbAssocNetworkSurfaceActionBody  Action that sets the contents of AcDbLoftedSurface entity created by surface network operation   
 AcDbAssocObjectActionParam  Action parameter that just keeps a dependency on an AcDbObject. It does not add any more functionality as compared to directly using an AcDbAssocDependency, but it may add consistency to custom action body code that may keep all the data in the form of action parameters and access the data through the action parameters. It also allows the action body to use the action parameter name (string) to get to the referenced AcDbObject.  
 AcDbAssocOffsetSurfaceActionBody  Action that sets the contents of the AcDbSurface entity created by the surface offset operation.
The action can be fully associative or semi-associative. If fully associative, the resulting surface will always automatically regenerate when any input data is changed. If semi-associative, when the geometry of input surfaces is changed, this action will be erased and the resulting surface will not regenerate and become a generic surface.  
 AcDbAssocParamBasedActionBody  AcDbAssocParamBasedActionBody allows the client code to keep and access its data in a uniform an high-level way. Instead of each concrete custom action body devising its own way of keeping its data, the data can uniformly be stored in the form of AcDbAssocActionParam(eters), and for simple numerical values, in the form of value action parameters. The action parameters take care of keeping the data in various ways, and provide the current values to the action body anytime the action body requests them. The action body does not need to care about any of this, and it even does not need... more  
 AcDbAssocPatchSurfaceActionBody  An action that sets the contents of an AcDbSurface entity created by the surface patch operation.
A patch surface can be curve-based or subentity edge-based. For a curve-based patch surface no continuity or bulge information is necessary. The action can be fully associative or semi-associative. If fully associative, the resulting surface will always automatically regenerate when any input data is changed. If semi-associative, for subentity edge-based patch surfaces, when input surfaces are detached from the resulting surface, this action will be erased and the resulting surface will not regenerate and become a generic surface. For curve-based patch surfaces, when the... more  
 AcDbAssocPathActionParam  Action parameter that maintains a sequence of edges. It is an AcDbAssocCompoundActionParam that owns AcDbAssocEdgeActionParams that represent the individual segments of the path.  
 AcDbAssocPathBasedSurfaceActionBody  Base action body class for surface creation actions that take as input one or more paths. It just provides utility methods to get/set the AcDbAssocPathActionParams.   
 AcDbAssocPersSubentId  Abstract base class used to persistently identify an AcDbSubentId of an AcDbEntity. Unlike AcDbSubentId that is transient and may change when the entity is modified, reevaluated, or similar, the AcDbAssocPersSubentId identifies the subentity in such a way that it is always possible to obtain the corresponding subentities (if they still exist), even after the entity has been edited or reevaluated any number of times.
The base AcDbAssocPersSubentId class is a pure virtual base class. Concrete AcDbEntity classes need their own way of representing a persistent identification of their subentities. Therefore, there are derived AcDbAssocPersSubentId classes that keep the persistent identification... more  
 AcDbAssocPersSubentIdPE  This AcRx protocol extension is a pure virtual base class that defines the protocol for obtaining a persistent AcDbAssocPersSubentId from a transient AcDbSubentId. It also adds subentity queries and manipulation protocols not available in the AcDbEntity class proper so that the client code can manipulate subentities of entities.
The derived concrete classes implement this protocol for the individually derived AcDbEntity classes.  
 AcDbAssocPlaneSurfaceActionBody  Action that sets the contents of an AcDbPlaneSurface entity based on the input data of the action. With this action, when the input edges used to create a plane surface are changed, the plane surface will regenerated  
 AcDbAssocRevolvedSurfaceActionBody  Action that sets the contents of an AcDbRevolvedSurface entity based on the input data of the action. With this action, if the original input data is changed, the revolved surface will regenerate.  
 AcDbAssocSimplePersSubentId  This concrete class, derived AcDbAssocPersSubentId, is for persisting AcDbSubentId that do not change.  
 AcDbAssocSingleEdgePersSubentId  The concrete derived AcDbAssocPersSubentId class that identifies an edge subentity if the entity has only a single edge subentity (such as in the case for curve entities).  
 AcDbAssocSurfaceActionBody  Base action body class for surface modeling actions whose result is a single surface. The base class just keeps a dependency on the resulting surface and the "is semiassociative" flag.   
 AcDbAssocSweptSurfaceActionBody  Action that sets the contents of an AcDbSweptSurface entity based on the input data of the action. With this action the swept surface will regenerate if any of the original input data is changed.  
 AcDbAssocTrimSurfaceActionBody  Action used to record a surface trimming operation, so it can be used to retrim the surface when blank/tool are changed. The trim action can be associative or semi-associative(permanent): when it is associative, the trimmed area will update when the tools/blank are changed by using the modified tools/blank to re-calculate the trimmed area. If it is semi-associative(permanent), the trimmed area will stay unchanged even if the tools/blank are modified because it will remember the geometry of the tools when first used, which will always be used to trim the blank  
 AcDbAssocValueDependency  A concrete dependency class that represents a dependency on a scalar value, such as on a numerical variable. The dependent-on object needs to expose the AcDbAssocValueProviderPE protocol extension class that is used to obtain the value from the object. It is possible to specify the name of the value in case the object provides more than one value.  
 AcDbAssocValueProviderPE  AcRx protocol extension pure virtual base class that defines protocol to get and set arbitrary simple scalar values of AcDbObjects. The values are identified by string names (needed if there is more than one of them for the same object) and their meaning is up to the implementer of the concrete protocol extension derived classes.
The derived concrete classes implement this protocol for the individual AcDbObject classes that want to offer some named values. The immediate clients of this protocol are AcDbAssocVariable class that exposes this interface to provide the value of the variable and AcDbAssocValueDependency class that uses this... more  
 AcDbAssocVariable  The variable keeps a scalar value such as a double, int, or a string. The value can be either a constant or be defined by an expression. The expression can reference other symbols. The AcDbAssocVariable then has AcDbAssocValueDependencies on these referenced symbols. Conversely, there may be AcDbAssocValueDependencies on this variable to obtain the value of the variable.
The evaluate() method of the variable evaluates the expression using the current values of the referenced symbols and sets the evaluated value.  
 AcDbAssocVariableCallback  Client code can register callback(s) derived from AcDbAssocVariableCallback that are then called when some events are triggered for AcDbAssocVariables.  
 AcDbAssocVertexActionParam  Action parameter that maintains a reference to a vertex subentity of an AcDbEntity, or directly maintains a AcGePoint3d geometry.  
 AcDbAttribute  The AcDbAttribute class represents the ATTRIB entity within AutoCAD. ATTRIB entities are associated with INSERT entities (AcDbBlockReference objects).  
 AcDbAttributeDefinition  The AcDbAttributeDefinition class represents the ATTDEF entity within AutoCAD. ATTDEF entities are used within block definitions (AcDbBlockTableRecords) for AutoCAD to use as templates for creating ATTRIB entities (AcDbAttribute objects) when an INSERT (AcDbBlockReference) is created that references the block definition.  
 AcDbAuditInfo  This class tracks the basic audit information during a database Audit operation.  
 AcDbAuditRecreatePE  This class defines the Runtime Extension Protocol for audit-time object recreation.
The contract is rather simple, although the implementation may be less so. Member functions are invoked from the AcDb library, in the context of RECOVER and/or AUDIT. The contract is:
When an object of a given class is requested via AcDbAuditInfo::fetchObject, and is not valid, then first, the PE object's replace() member is invoked. If it returns Acad::eOk and a new instance of a class, then that instance is placed in the original object ID, and the repair is finished.
If that fails, then the PE object's redirect()... more  
 AcDbBackground  Base class of all background objects that describe a viewport background. There are three types of concrete background types: AcDbSolidBackground
, AcDbGradientBackground and AcDbImageBackground. Each object should reside in the ACAD_BACKGROUND dictionary. If a tilemode viewport or a paperspace viewport has a background, it has to refer to one of these classes.

The application that sets a viewport background has the following responsibilities: 
Create a new background, give it a unique name, set appropriate properties and append it to the ACAD_BACKGROUND dictionary. 
Remove the previous background object from the dictionary (if it does not require it anymore). 
  
 AcDbBlendOptions  Utility class to specify options for blend surfaces.  
 AcDbBlockBegin  The AcDbBlockBegin class represents the block header portion of a block definition within AutoCAD (that is, group codes 2, 3, 10, 20, 30, and 70). Objects of this class are created and handled automatically by AutoCAD for each AcDbBlockTableRecord.
Applications do not need to create objects of this class nor can they add or remove them from databases. Applications are free to add or manipulate xdata on objects of this class as well as work with extension dictionaries for objects of this class.  
 AcDbBlockChangeIterator  This class is used for iteration through modified entities. The access of the internal modified list data for custom Index objects is read-only. It is created and passed in to the AcDbIndex::rebuildModified() method.  
 AcDbBlockEnd  The AcDbBlockEnd class represents the ENDBLK object within AutoCAD. Objects of this class are created and handled automatically by AutoCAD for each AcDbBlockTableRecord.
Applications do not need to create objects of this class nor can they add or remove them from databases. Applications are free to add or manipulate xdata on objects of this class, as well as work with extension dictionaries for objects of this class.  
 AcDbBlockInsertionPoints  This class allows applications to elaborate different insertion points for blocks. 
  
 AcDbBlockReference  The AcDbBlockReference class represents the INSERT entity within AutoCAD. A block reference is used to place, size, and display an instance of the collection of entities within the AcDbBlockTableRecord that it references. In addition, block references can be the owner of AcDbAttribute entities (the list of which is automatically terminated by an AcDbSequenceEnd entity).
AcDbBlockReference set functions inherited from AcDbEntity ignore the doSubents argument because these subentities are attributes that are really special text entities and need to have their own individual properties (unlike polyline vertices, which are just data points).
  
 AcDbBlockReferenceIdIterator  Objects of the AcDbBlockReferenceIdIterator class are created by calling the AcDbBlockTableRecord::newBlockReferenceIdIterator() method on an open AcDbBlockTableRecord. The iterator will traverse through the list of AcDbBlockReferences that are currently referencing the AcDbBlockTableRecord that created the iterator.  
 AcDbBlockTable  This class is the symbol table for AcDbBlockTableRecords, which represent block definitions within a drawing database.
  
 AcDbBlockTableIterator  This class is for iterating over the records in an AcDbBlockTable.
The only substantial difference from the base class AcDbSymbolTableIterator is the use of signatures specific to AcDbBlockTableRecord for the member functions getRecord() and seek().  
 AcDbBlockTableRecord  Objects of the AcDbBlockTableRecord class are used as containers for entities within drawing file databases. AcDbBlocktableRecord objects (often referred to as BTRs) are owned by the database's AcDbBlockTable object. The BTRs in turn own the entity objects they contain.
There are two special BTRs that are always present in every database. They are *MODEL_SPACE and *PAPER_SPACE. They are the Model and Paper Spaces for the database. Any entity that is created by AutoCAD while in Model Space is owned and contained by the *MODEL_SPACE BTR. Entities created while in Paper Space go into the *PAPER_SPACE BTR.
BTRs other than *MODEL_SPACE... more  
 AcDbBlockTableRecordIterator  This iterator is returned by the function AcDbBlockTableRecord::newIterator(). It is used to iterate through an AcDbBlockTableRecord.  
 AcDbBody  The AcDbBody class is used to represent nonmanifold solids and other ShapeManager entities that do not fit the characteristics of the AcDb3dSolid or AcDbRegion classes.  
 AcDbBreakData  This class enables the application to get or set dimension break data.
  
 AcDbBreakPointRef  This class enables the application to get or set dimension break point.
  
 AcDbCamera  The camera entity represents a named view in model space. Camera entities are not persisted into the drawing file. A camera entity is created when the CAMERADISPLAY sysvar is set to 1. When a named view is deleted its corresponding camera is also deleted, and vice versa.
Rather than creating a camera entity directly, it is recommended instead that you simply create a named view.
By default, camera entities are created on a hidden layer. Moving a camera entity to another layer is not recommended.
  
 AcDbCircle  The AcDbCircle class represents the circle entity within AutoCAD.  
 AcDbColor  For internal use only.
  
 AcDbCompositeFilteredBlockIterator  This class is used to process a query on an index. It is used with AcDbBlockTableRecord.  
 AcDbCompoundObjectId  This class is used to identify an AcDbObject in an AcDbDatabase when the object is referenced via a path of AcDbBlockReferences. The object may reside in the host database (the same database as the AcDbCompoundObjectId belongs to) or it may reside in an XREF database or even in an unrelated database.
Notice that if the AcDbCompoundObjectId references an object in another database, reactors will be created to keep track of relevant events that happen to the database, such as database deletion or XREF unload/reload.  
 AcDbCurve  The AcDbCurve class is the base class for all the entity classes that are variations of a curve such as AcDbArc, AcDbCircle, AcDbEllipse, AcDbSpline, and others. This base class provides the common functionality such as finding parameters for a point on the curve, finding offset curves, finding projections of the curve onto a plane, and so on.  
 AcDbCurvePath  This class defines a curved path.
  
 AcDbCustomOsnapInfo  The class AcDbCustomOsnapInfo defines protocol that every custom OSNAP mode will implement for every relevant entity class. The specific class for a custom OSNAP mode must be derived from this class and then registered with AcRx. Actual implementations for a given custom OSNAP mode must then be derived from that explicitly registered class, and then associated with appropriate AcRxClass objects as is done with protocol extension.
During processing, when a given custom OSNAP mode is active, its corresponding protocol extension class is used to look up the appropriate protocol extension object for each entity. The object is then cast... more  
 AcDbCustomOsnapManager  AcDbCustomOsnapManager defines the manager object for registered custom OSNAP modes. Custom modes are registered and deregistered through this object, they can be activated and deactivated, and the set of currently active modes can be queried through this object.  
 AcDbCustomOsnapMode  A custom OSNAP mode is defined by registering an instance of AcDbCustomOsnapMode with the AcDbCustomOsnapManager object.
Whenever a keyword representing a built-in OSNAP mode can be used, the keyword of any of the registered custom OSNAP modes can also be entered. These places include:

establishing an OSNAP override whenever a point is being acquired. 
establishing the equivalent of running OSNAP modes via the -OSNAP command as explained in AutoCAD User Guide and Reference. 
invoking the AutoLISP (OSNAP) function or the ObjectARX acedOsnap() function. 

It can be specified as a running mode via the OSNAP command, setting the OSNAP system... more  
 AcDbCustomOsnapMode2  This class extends AcDbCustomOsnapMode to a support custom osnap menu item icon and display string.  
 AcDbDatabase  The AcDbDatabase class represents the AutoCAD drawing file. Each AcDbDatabase object contains the various header variables, symbol tables, table records, entities, and objects that make up the drawing.
The AcDbDatabase class has member functions to allow access to all the symbol tables, to read and write to DWG files, to get or set database defaults, to execute various database-level operations such as wblock and deepCloneObjects, and to get or set all header variables.  
 AcDbDatabaseReactor  The AcDbDatabaseReactor class, if defined, allows notification to be issued when an object is appended, modified, or erased from a drawing database.  
 AcDbDatabaseSummaryInfo  The AcDbDatabaseSummaryInfo class is a pure virtual class for encapsulating a set of character strings that can be used to add additional information to a DWG file. The maximum length of these strings is dependent on several items, but will always be at least 1,000 characters. This information is stored and retrieved in the SummaryInfo object with specific methods for each information field.
You may create your own custom fields in addition to the predefined fields. These custom fields are stored in a list, and you can manipulate custom fields by either their name (or key) or by their position... more  
 AcDbDataCell  The AcDbDataCell class provides a way to represent the following data types in a generic manner: Boolean, integer, double, char*, AcGePoint3d, AcDbObjectId, AcDbHardOwnershipId, AcDbSoftOwnershipId, AcDbHardPointerId, and AcDbSoftPointerId. It provides a full complement of copy constructors, assignment operators, and cast operators to make it simple and safe to use AcDbDataCell cells as an underlying type.
An AcDbDataCellArray is a typedef for an AcArray of AcDbDataCell objects. An AcDbDataCellArray is used as an argument to various AcDbDataTable functions to pass row data. It is also the underlying structure for the AcDbDataColumn class. The functions of... more  
 AcDbDataColumn  The AcDbDataColumn class represents a single column of an AcDbDataTable.
The column consists of a zero-based array of AcDbDataCell objects of the same type. The cell types are those contained in the AcDbCellData::CellType enumeration. Functions for inserting and appending cells to an AcDbDataColumn enforce this type constraint. 
An AcDbDataColumn can have an optional name.
  
 AcDbDataLink  This class is used to manage the connection information for establishing a data link between a table in a drawing to an external data source.
  
 AcDbDataLinkManager  This class is used to manage the data links in a database. An instance of this class can be obtained from the database using AcDbDatabase::getDataLinkManager().  
 AcDbDataTable  The AcDbDataTable class provides a general-purpose repository for simple numeric, character, 3D point, or object ID information arranged in a two-dimensional table. It relies on two component classes: AcDbDataCell and AcDbDataColumn.
The AcDbDataTable class represents an array of AcDbDataColumn objects. A row of data is represented by an AcDbDataCellArray, which is a typedef for an AcArray of AcDbDataCell objects. The array is zero based, as is the column array, so the first cell in a table is referenced by indexes of [0,0].
An AcDbDataTable can be named.
To build a table, append the desired columns and add content... more  
 AcDbDate  This class is used to pass date and time information to or from various AutoCAD ObjectARX functions.
This class exports a conventional representation of date and time as well as a Julian date and time representation. AutoLISP uses Julian dates. For reference, see the DATE system variable in the AutoCAD Command Reference.  
 AcDbDeepCloneFiler  This class maintains a queue of typed (for instance, AcDbHardOwnershipId, AcDbSoftOwnershipId, and so on) object IDs. All read and write requests are forwarded to an embedded "real" filer, except for object IDs, which are removed from or added to the typed object ID queue. The class methods provide protocols for reading or writing "owned" objects in the queue. (The deepClone mechanism follows ownership references.)  
 AcDbDetailSymbol  The AcDbDetailSymbol class representing the detail symbol entity created by the model documentation feature.  
 AcDbDetailViewStyle  The style object for a detail view.  
 AcDbDgnDefinition  Represents DGN underlays in the drawing. Underlays are similar to raster images but their content is snappable.  
 AcDbDgnReference  Represents DGN underlays in the drawing. Underlays are similar to raster images but their content is snappable.  
 AcDbDgnRefFileRepath  
An object of this class is used to change the paths of reference files in DGN files.  
 AcDbDgnUnderlayItem  Underlay items.  
 AcDbDiametricDimension  The AcDbDiametricDimension class represents the diameter dimension type in AutoCAD.
This dimension type requires two points that define a diameter chord on the curve being dimensioned to be able to draw the dimension line from one chord point to the other. In addition, if the text is located outside the curve being dimensioned, then a "leader length" value is used to determine how far the dimension line extends out past the curve before doing a horizontal dogleg (if necessary) to the annotation text.  
 AcDbDictionary  AcDbDictionary is a database-resident object dictionary, which maintains a map between text strings and database objects. An instance of this class represents a single object, such as Drawing Symbol Table, to which objects derived from AcDbObject may be added, accessed, and removed. Entries in an AcDbDictionary must be unique. Entries consist of a unique AcDbObject and string, which comprises the entry's key name. The key may be either an explicit null-terminated text string, or an asterisk (' * ') as the first character in the string to signify an anonymous entry. An anonymous entry's key will be constructed internally by... more  
 AcDbDictionaryIterator  This class provides the ability to iterate through an AcDbDictionary.  
 AcDbDictionaryWithDefault  This class enhances the AcDbDictionary class by adding the concept of a 'default' entry. Each AcDbDictionaryWithDefault has an AcDbHardPointerId data member that is used to store the object ID of one of its entries. This hard pointer ID is used as the default entry. Get and set methods are provided for accessing this hard pointer.  
 AcDbDimAssoc  
This class is the main class used to associate dimensions to geometry.  
 AcDbDimData  The AcDbDimData class supports dynamic dimensions for objects derived from AcDbEntity.
Dynamic dimensions are temporary AcDbDimension objects that can be invoked and displayed when new database entities or grip-editing database-resident entities are created. AcDbDimData objects are usually created by an application or custom object and displayed and managed by AutoCAD.
When an application implements the AcEdJig::dimData() method or the AcDbGripData::hotGripDimensionFunc() or AcDbGripData::hoverDimensionFunc() callback function, it returns a pointer to an AcDbDimDataPtrArray of pointers to AcDbDimData objects.
  
 AcDbDimension  The AcDbDimension class is the base class for the classes that represent all the dimension entity types within AutoCAD. The appearance of dimensions is controlled by dimension variable settings and dimension styles.  
 AcDbDimensionStyleOverrule  AcDbDimensionStyleOverrule overrules a subset of dimension style related operations that AcDbDimension class specifies. It is intended as a base class for clients who want to alter some or all behavior of a given AcDbDimension-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.   
 AcDbDimStyleTable  This class is the symbol table for AcDbDimStyleTableRecords, which represents dimension styles within the drawing database.  
 AcDbDimStyleTableIterator  This class is for iterating over the records in an AcDbDimStyleTable.
The only substantial difference from the base class AcDbSymbolTableIterator is the use of AcDbDimStyleTableRecord-specific signatures for the member functions getRecord() and seek().  
 AcDbDimStyleTableRecord  Objects of this class represent the records found in the AcDbDimStyleTable. Each of these records contains the information necessary to generate a specific appearance (that is, text above, in, or below the line; arrows, slashes, or dots at the end of the dimension line, and so on) for dimensions that reference it.  
 AcDbDisableAnnoAutoScale  AcDbAnnotationScale is a helper class. By instantiating a local variable to use this class, the constructor of the class saves off the current value of the sysvar, ANNOAUTOSCALE, and sets ANNOAUTOSCALE to zero. Then, when the local variable goes out of scope, the destructor of this class returns ANNOAUTOSCALE to its original value.  
 AcDbDwfDefinition  Represents DWF underlays in the drawing. Underlays are similar to raster images but their content is snappable. 
  
 AcDbDwfReference  Represents DWF underlays in the drawing. Underlays are similar to raster images but their content is snappable. 
  
 AcDbDwgFiler  This abstract class provides the interface for derived classes that are used for I/O operations that work with the DWG information format.
Specialty internal classes (that is, not exported to the API) derived from this class are used for DWG filing and Undo recording. In addition, the AcDbDeepCloneFiler and AcDbWblockCloneFiler classes in the API are derived from this class to handle deepClone and wblockClone filing operations.  
 AcDbDxfFiler  This abstract class provides the interface for derived classes that are used for I/O operations that work with the DXF information format.
Specialty internal classes (that is, not exported to the API) derived from this class are used for DXF filing and for AutoLISP ADSRX function interaction with objects.  
 AcDbDynamicUCSPE  AcDbDynamicUCSPE Protocol extension base class.
Developers can derive from this class in order to enable Dynamic UCS switching for their custom objects or for other core objects. By default, the core DUCS code will call this protocol extension on objects under the cursor during the initial point acquisition in a DUCS-enabled command. Derived classes must override and implement getCandidatePlanes().  
 AcDbDynBlockReference  A utility class for accessing information about block references to dynamic blocks.
  
 AcDbDynBlockReferenceProperty  A utility class describing a property on a dynamic block, including getting and setting the current value on the dynamic block. The AcDbDynBlockReferenceProperty class is a reference to a property defined on a dynamic block instance, and can be used to get and set the current value on that instance. 
  
 AcDbDynBlockTableRecord  
A utility class for accessing information about dynamic block definitions and managing the associated anonymous blocks.  
 AcDbEdgeRef  Reference to an edge, i.e. a subentity whose geometry is a simple curve.
It may either be the whole entity that itself is a single curve, kEdgeSubentType subentity of an entity, or a constant AcGeCurve3d. Notice that this reference is generally non-persistent, because it uses transient AcDbSubentId. The option to reference the whole entity is questionable and has been provided just for the compatibility with old code that takes the whole entity. We should probably always represent edges by kEdgeSubentType subentities of entities, even if the whole entity is just a single edge, such as line, arc, or circle.  
 AcDbEllipse  This class represents the ELLIPSE entity in AutoCAD. It contains the methods to create, modify, and obtain properties of the ellipse.  
 AcDbEntity  Base class for all database objects having a graphical representation.  
 AcDbEntityHyperlinkPE  AcDbEntityHyperlinkPE is the hyperlink protocol extension class. The methods of the AcDbEntityHyperlinkPE class allow you to set, get, and count the hyperlinks associated with an entity. The information associated with hyperlinks consists of the name of the link, a description of the link, and a sublocation within the link. For AutoCAD, a sublocation is a named view, while in a spreadsheet application, for example, a sublocation might be a cell or group of cells.  
 AcDbEntityReactor  The AcDbEntityReactor class is designed to be used as a base class for custom reactor classes that want to receive notification when an entity object has had one of its modification methods called and has then been closed in order to commit any changes made. This reactor type can be attached only to entities, so it provides a way to distinguish changes made to entities versus changes made to any object.  
 AcDbEvalConnectable  This class implements a runtime inspection interface for named properties on AcDbEvalExpr objects. Objects implementing this class expose a collection of named, typed properties and semantics for connecting properties between AcDbEvalGraph-resident connectable instances.
  
 AcDbEvalContext  This class implements a simple container for application data that can be used during the evaluation of an AcDbEvalGraph. The graph passes any AcDbEvalContext object supplied in a call to AcDbEvalGraph::evaluate() to each node in the graph when calling AcDbEvalExpr::evaluate() during the ensuing traversal. Graph client applications typically use the context to store application-specific data used by custom nodes during their evaluation.
  
 AcDbEvalContextIterator  This class provides access to the contents of an AcDbEvalContext collection.
AcDbEvalContextIterator instances are returned by calls to AcDbEvalContext::newIterator().
  
 AcDbEvalContextPair  This class represents a single entry in an AcDbEvalContext container.
This is a simple key-value pair stored in an AcDbEvalContext. The values are void pointers. Neither this class nor the AcDbEvalContext class is responsible for managing the memory allocated to the values stored in the context.
  
 AcDbEvalEdgeInfo  This utility class holds directed edge information between two nodes in an AcDbEvalGraph. Instances of this class are returned by calls to edge methods on AcDbEvalGraph.  
 AcDbEvalExpr  This class represents a single node in an AcDbEvalGraph. The node represents an action (or expression). The owning graph calls the node's evaluate() method when visiting the node during a traversal of the graph from within the graph's evaluate() method.
  
 AcDbEvalGraph  This class holds the network of interrelated elements that implement the behavior of dynamic blocks. Each individual element (or AcDbEvalExpr) can depend on zero or more other AcDbEvalExpr objects. The role of AcDbEvalGraph is to invoke the AcDbEvalExpr::evaluate() method for each of the elements it contains at the proper time. Before an AcDbEvalExpr can be evaluated, evaluate() must be invoked for any dependent AcDbEvalExpr objects. AcDbEvalGraph uses a directed acyclic graph (DAG) to reprsent the dependencies between AcDbEvalExpr objects. If an AcDbEvalExpr E1 depends on (requires input from) an AcDbEvalExpr E2, an edge from E2 to E1 is represented... more  
 AcDbEvalIdMap  This class is used by AcDbEvalExpr::remappedNodeIds() to map old node IDs to new node IDs.
  
 AcDbEvalVariant  This class provides a lightweight wrapper for a resbuf structure. It provides typed constructors and overloaded assignment operators to facilitiate assigning values to the underlying data. AcDbEvalExpr objects return instances of this class for the result of the expressions.
The class manages the copying of strings by calling acutNewString() to copy strings. Linked lists of resbufs are not directly supported, but if an AcDbEvalVariant contains a linked resbuf chain the destructor frees the entire chain using acutRelRb().
  
 AcDbExtents  This class was designed to be used with the getGeomExtents() function of AcDbEntity-derived classes. It embodies a box in 3D space whose edges are parallel to the axes of the WCS. This box is represented in the private data of an AcDbExtents object as a minimum point (minPoint) and a maximum point (maxPoint). At all times except immediately after an AcDbExtents object has been instantiated with the default constructor (see below), each of the X, Y, and Z values of minPoint must be <= the corresponding values of maxPoint.  
 AcDbExtents2d  This class embodies a rectangle in 2d space whose edges are parallel to the X and Y axes of the WCS. This rectangle is represented in the private data of an AcDbExtents2d object as a minimum point (mMinPoint) and a maximum point (mMaxPoint). At all times except immediately after an AcDbExtents2d object has been instantiated with the default constructor, the mMinPoint X and Y values must be less than or equal to the corresponding X and Y values of mMaxPoint.  
 AcDbExtrudedSurface  This class represents an extruded surface entity. It provides methods to create and manipulate ASM surfaces.  
 AcDbFace  The AcDbFace class represents the 3dface entity within AutoCAD.  
 AcDbFaceRecord  The AcDbFaceRecord class is a special class of vertex that is used by the AcDbPolyFaceMesh class to group together mesh point vertices that represent faces in the mesh. Each AcDbFaceRecord stores the vertex indices of the vertices that are the face's corner points and the visibility information for the edges determined by the vertices.  
 AcDbFaceRef  Reference to a face. It can either be represented by a face AcDbSubentId of an ASM-based AcDbEntity (AcDbSurface, AcDb3dSolid, AcDbRegion) or by a constant ASM BODY and the AcDbSubentId then specifies the index of the face in the constant ASM BODY.  
 AcDbFcf  The AcDbFcf class represents Feature Control Frames for Geometric Dimensioning and Tolerancing. It is the entity type created by the TOLERANCE command in the AutoCAD command editor. Tolerances are considered as dimensions in AutoCAD, which means they are controlled by dimension variable settings and dimension styles.
Within objects of this class, the text string data member contains symbol and format codes that are used to define the actual graphical representation that will be seen within AutoCAD. For more information on these control codes, see the AcDbFcf::setText() member function description.  
 AcDbField  This class is used to represent a field. The field can evaluate to one of the supported data types. The field object acts as a container to store the field expression, evaluated result, and other data. An evaluator is required for evaluating the field.
  
 AcDbFilter  This class defines a "query." It provides the "key" to the AcDbCompositeFilteredBlockIterator, for which the corresponding index is obtained through the indexClass() method.
The AcDbSpatialFilter::indexClass() method will return AcDbSpatialIndex::desc().
Applications that need to provide their own indexing scheme need to provide versions of these three classes: 
AcDbIndex 
AcDbFilter 
AcDbFilteredBlockIterator 
  
 AcDbFilteredBlockIterator  This class provides a method to process a query on an index. Used by the AcDbCompositeFilteredBlockIterator class.  
 AcDbFormattedTableData  This class derives from AcDbLinkedTableData and adds formatting support for the cells, rows, and columns in the table. This class contains formatting information for display of the table.
  
 AcDbFrame  Abstract base class for OLE 2 container features (AcDbOleFrame and AcDbOle2Frame).  
 AcDbFullSubentPath  This class is used for uniquely identifying a subentity within a particular entity. An object of this class consists of array of object IDs and an AcDbSubentId object.
The subentity ID object contains the graphics system marker of the subentity and the type of the subentity (that is, edge, face, vertex). The object ID array contains a sequential list of the object IDs for all the objects that make up the "path" from the outermost entity (in Model or Paper space) down into the "main" entity that the subentity is a part of. If the subentity's "main" entity is directly... more  
 AcDbGeoCompoundCoordinateSystem  The AcDbGeoCompoundCoordinateSystem class represents a compound coordinate reference system that consists of a horizontal geodetic coordinate system and a vertical coordinate system.  
 AcDbGeoCoordinateSystem  The AcDbGeoCoordinateSystem class represents a coordinate reference system (CRS) definition. A CRS is also being referred to as a spatial reference system (SRS).  
 AcDbGeoCoordinateSystemCategory  The AcDbGeoCoordinateSystemCategory class represents a category that holds string identifiers (IDs) referring to a group of coordinate reference systems  
 AcDbGeoCoordinateSystemTransformer  The AcDbGeoCoordinateSystemTransformer class represents a transformer object which is used to transform points from the source CRS to the target CRS.  
 AcDbGeoData  This object identifies the geographical space a design is located in. It also provides all the information necessary to accurately map a design coordinate system to a location on earth (a latitude-longitude based system). Furthermore this object provides a simple transformation to support efforts like Google Earth and geo-tagging and enough information to initialize geo-spatial applications for accurate coordinate system transformation and place a design on the earth, relative to each other.
The minimum requirements for a valid object are: 
Type of design coordinates. 
Design point. 
Reference point. 
A "design coordinate to geodetic coordinate" mesh with at least one point.... more 
 
 AcDbGeoDataReactor  This class serves as a base class for custom classes that receive notification of changes to the geo coordinate system that are made by the AcDbGeoData::setCoordinateSystem() API.  
 AcDbGeoMap  
The AcDbGeoMap class represents an embedded raster image which is captured from a portion of LiveMap  
 AcDbGeometryOverrule  Derive from AcDbGeometryOverrule to overrule a subset of geometry related operations implemented in the AcDbEntity class. It is intended as a base class for clients who want to alter some or all behavior of a given AcDbEntity-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcDbGeomRef  Base class for the concrete derived classes like AcDbEdgeRef, AcDbPathRef and AcDbVertexRef. These are simple classes that keep a reference to a subentity of an entity and can represent this subentity information in various ways. They are used mainly to pass around information about geometry of a subentity or of an entity, not to keep this information.
The subentity references are non-presistent. Anytime the referenced entity is changed or re-evaluated, the subentity reference becomes invalid. Persistent references can be established using AcDbAssocEdgeActionParam, AcDbAssocPathActionParam, AcDbAssocVertexActionParam, or directly using AcDbAssocGeomDependencies.  
 AcDbGeoPositionMarker  Missing 
The AcDbGeoPositionMarker class represents a geographical location aware marker object with label.  
 AcDbGeoVerticalCoordinateSystem  The AcDbGeoVerticalCoordinateSystem class represents a vertical coordinate reference system (vertical CRS) definition. A vertical CRS is also being referred to as a spatial reference system (SRS).  
 AcDbGradientBackground  This class represents a gradient background.
  
 AcDbGraph  This class provides a generic graph container implementation. Two other classes are also involved in the graph mechanism: AcDbGraphNode and AcDbGraphStack.
Detection for circular references is done by internally creating a duplicate set of references in each node and then trimming away all leaf nodes, which terminate without circularity. If any nodes remain in the duplicate graph, those nodes exist in a cycle. AcDbGraph::findCycles() is used to set up the internal cycle information and enable several query methods to return information about any cycles found.  
 AcDbGraphNode  This class provides a generic graph node implementation. Two other classes are also involved in the graph mechanism: AcDbGraph and AcDbGraphStack.
Detection for circular references is done by internally creating a duplicate set of references in each node, then trimming away all leaf nodes which terminate without circularity. If any nodes remain in the duplicate graph, those nodes exist in a cycle. AcDbGraph::findCycles() is used to set up the internal cycle information and enable several query methods to return information about any cycles found.  
 AcDbGraphStack  Stack class for AcDbGraphNode objects.  
 AcDbGripData  This class describes a specific grip for a specific entity. It allows applications with control over grips to

Draw their own grip glyph graphics 
Perform custom operations when the user picks or hovers over a grip 
Control a right-click menu that involves grips 
Receive detailed notification about a grip editing operation 

When a custom class implements the AcDbEntity::getGripPoints() function, it fills the AcDbGripDataArray argument with AcDbGripData objects, one per grip point.
The AcDbGripData object contains two members that define the grip: the actual 3d point for the grip and a pointer to a data structure specified by the custom entity... more  
 AcDbGripOverrule  AcDbGripOverrule overrules a subset of grip related operations that AcDbEntity class specifies. It is intended as a base class for clients who want to alter some or all behavior of a given AcDbEntity-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcDbGroundPlaneBackground  This class represents a ground plane background.
  
 AcDbGroup  The AcDbGroup class represents a collection of entities referred to by a single name. All AcDbGroup objects belong to a dictionary object which can be obtained through the AcDbDatabase::getGroupDictionary() method.
Unlike a block, entities within a group can be individually manipulated.
This class contains a dynamic array of object IDs that are the objects in the group.  
 AcDbGroupIterator  This class is for iterating over objects in an AcDbGroup.  
 AcDbHandle  The AcDbHandle class is the ObjectARX class designed to encapsulate the 64-bit integer handle identifiers that have been in AutoCAD for several releases before R13. A handle uniquely identifies an AcDbObject within a single database and is saved with that object in the database so that it persists from session to session. Handles are not unique across databases, however. In fact, duplication across databases is almost a certainty, since all databases start with the same handseed value of 1 and go up from there.  
 AcDbHardOwnershipId  This class is a specialization of AcDbObjectId. It is used to distinguish an object specified by an AcDbHardOwnershipId as being hard owned by some other object. Other properties are the same as the more generic AcDbObjectId.  
 AcDbHardPointerId  This class is a specialization of AcDbObjectId that is used to distinguish an object specified by an AcDbHardPointerId as being hard pointed to by some other object. Other than that, the properties are the same as the more generic AcDbObjectId.  
 AcDbHatch  AcDbHatch is a planar entity that can be created and placed in an arbitrary plane in 3D space.

The hatch plane can be uniquely defined by a normal vector in WCS (World Coordinate System) and an elevation indicating the distance from the WCS origin to the hatch plane. The hatch plane adopts a standard AutoCAD object coordinate system (OCS). Its origin coincides with the WCS origin and its X and Y axes are calculated using the arbitrary axis algorithm.

The hatch boundary defines the area to be filled with the specified hatch pattern. The internal representation of the hatch boundary... more  
 AcDbHelix  This class implements the AutoCAD helix entity. AcDbHelix derives from AcDbSpline, but overrides the spline properties with its own behavior. Objects of the AcDbHelix class contain an embedded AcDbSpline which is wrapped by the AcDbHelix in order to constrain its behavior so that its basic shape is maintained. The behavior of calling NURB functions on the AcDbHelix class is unknown and not recommended.  
 AcDbHighlightOverrule  AcDbHighlightOverrule overrules a subset of highlight related operations that AcDbEntity class specifies. It is intended as a base class for clients who want to alter some or all behavior of a given AcDbEntity-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcDbHighlightStateOverrule  AcDbHighlightStateOverrule overrules a subset of highlight style related operations that the AcDbEntity class specifies. It is intended as a base class for clients that want to alter some or all behavior of a given AcDbEntity-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcDbHomeView  
This class objectifies the navigation home view.  
 AcDbHostApplicationProgressMeter  At various points, the database code will attempt to either create a temporary progress meter, or modify a single global progress meter. If you would like the database code to display its progress in your application, you should implement a progress meter by deriving from the AcDbHostApplicationProgressMeter class. This meter can display in any fashion you choose, but must implement the start(), stop(), meterProgress(), and setLimit() methods.
This class falls into the "may override" category. If you do not override the class, a default implementation that does nothing will be used by the database code. It is... more  
 AcDbHostApplicationServices  The AcDbHostApplicationServices class provides various services to host applications at runtime. These services are defined by the RealDWG client.
A RealDWG host application must provide a class derived from AcDbHostApplicationServices. A default service is not provided.
The controller and transaction methods of this class specify the undo controller and transaction manager that your application will use. You are not required to override or call these methods. The Internet methods allow your application to use URLs (Uniform Resource Locators) directly in your code or through a browser. The methods in the system functions group should generally not be overridden in... more  
 AcDbHyperlink  An AcDbHyperlink object contains the hyperlink name (for example, http://www.autodesk.com), a sublocation within that link, and the hyperlink description or friendly name (for example, "Click here for Autodesk's website").
See the ObjectARX Developer's Guide for an example of how to use this class.  
 AcDbHyperlinkCollection  This class is a collection of AcDbHyperlink objects. The AcDbHyperlinkCollection deletes its contents when they are removed, and when the collection object itself is deleted. Hyperlinks in the collection are numbered from 0.
See the ObjectARX Developer's Guide for an example of how to use this class.  
 AcDbIBLBackground  Image based lighting background.  
 AcDbIdMapping  AcDbIdMapping is used by the deep clone operation to map AcDbObjectIds between original objects and their clones (using AcDbIdPairs).  
 AcDbIdMappingIter  Objects of the AcDbIdMappingIter class are used to iterate through the ID map of an AcDbIdMapping object that is used in deep clone operations.  
 AcDbIdPair  AcDbIdPair is the element class for AcDbIdMapping, which is used in deep clone operations. It holds a pair of AcDbObjectIds. The key is the original object's ID, and the value is the cloned object's ID.  
 AcDbImage  Abstract base class for immediate mode raster-based entities, including AcDbRasterImage. Every time entities derived from this class are drawn on the screen or plotted, the getScanLines method is called by the display or plot driver to get the pixels needed for display in the image format and scale required by the driver. These objects must be drawn at regen time using the AcGiViewportGeometry::rasterImageDc() method accessible through AcGiViewportDraw::geometry() in the entity's viewportDraw method.
For more information about the AcGiRequestScanLines, refer to the RequestScanLines structure in the ADI 4.3 specification, but note that they are not identical. In particular, the... more  
 AcDbImageBackground  This class represents a background image.
  
 AcDbIndex  This is the base class for Index objects. AcDbSpatialIndex and AcDbLayerIndex derive from this class.
Keeping the index up-to-date is achieved through the AcDbIndexFilterManager::updateIndexes() call explicitly invoked (either by an application or AutoCAD)
The AcDbFilteredBlockIterator will serve as the means to visit all the AcDbObjectIds that are "hits" from the query defined by the AcDbFilter passed to its constructor. For example, in the Spatial index case, the AcDbSpatialFilter object instance passed to the newIterator() method will define a query region. The AcDbSpatialIndex object, through its newIterator() method, will provide an AcDbSpatialIndexIterator that will return IDs that correspond to entities... more  
 AcDbIndexUpdateData  This class is used to serve as a mapping between an AcDbObjectId and any index specific data or flags. For example, during spatial index update, one could associate an entity extents with the AcDbObjectId within this instance.
The primary purpose of the class's get and set methods is to give applications a non-intrusive yet efficient method (O(1)) of associating flags and data with an AcDbObjectId during an index update.
An AcDbIndexUpdateData instance is never directly instantiated. It is always supplied as an argument.  
 AcDbIndexUpdateDataIterator  This class is used for iteration through AcDbIndexUpdateData class objects.  
 AcDbInterferencePE  Protocol extension abstract base class.
  
 AcDbItemLocator  AcDbItemLocator, represents spatial index of an item in a 3D array i.e an item index{i,j,k} for rectangular array.   
 AcDbJoinEntityPE  AcDbJoinEntityPE is the joinEntity protocol extension class. The methods of the class allow you to join an entity or entities in the given primary entity. The entities that support this protocol extension include: AcDbLine, AcDbArc, AcDbPolyline, AcDb2dPolyline, AcDb3dPolyline, and AcDbSpline. Applications can implement this protocol extension for other entity types.  
 AcDbLayerFilter  This class is a list of layers that need to have their corresponding entity lists traversed during filtered block traversal. The newIterator() method does return a pointer to a valid AcDbFilteredBlockIterator object. These objects are optionally saved in the drawing for purposes of xref demand loading efficiency.  
 AcDbLayerIndex  This class is an index implementation especially for layers.  
 AcDbLayerIndexIterator  Provides a query on a layer index, by visiting entities on layers supplied by a AcDbLayerFilter object.  
 AcDbLayerStateManager  The AcDbLayerStateManager class is used to access and manipulate individual layer states associated with a drawing database. The layer state manager is not implemented as a collection class. Instead it is intended for high-level access to layer state manager functions. For enumerating, listing, and other lower-level functions, developers should use the existing APIs to access the layer state dictionary within the layer table's extension dictionary. The specific name of the layer state dictionary is ACAD_LAYERSTATES.
  
 AcDbLayerStateManagerReactor  This class provides notification of standard layer state operations.  
 AcDbLayerTable  This class is the symbol table for layers.
  
 AcDbLayerTableIterator  This class is for iterating over the records in an AcDbLayerTable.
The only substantial difference from the base class AcDbSymbolTableIterator is the use of AcDbLayerTableRecord-specific signatures for the member functions getRecord() and seek().  
 AcDbLayerTableRecord  Objects of this class represent records in the AcDbLayerTable. Each of these records contains the information (color, on or off, frozen or thawed, etc.) about a layer in the drawing database.  
 AcDbLayout  An AcDbLayout object stores characteristics of each paperspace layout. Layout objects are stored in an AcDbDictionary object with an ACAD_LAYOUT key, allowing easy iteration and indexing.  
 AcDbLayoutManager  This is an interface class for database-specific routines that manipulate and access AcDbLayout objects.  
 AcDbLayoutManagerReactor  The AcDbLayoutManagerReactor class is used as the base class for reactor classes that override methods called for events for which notification is desired. When an object of one of these derived classes is instantiated and added to the reactor chain of the layout manager, then whenever layouts are manipulated via the layout manager, the appropriate methods within the reactor object will be called.  
 AcDbLeader  The AcDbLeader class represents the LEADER entity within AutoCAD. Leaders are considered as dimensions in AutoCAD, which means they are controlled by dimension variable settings and dimension styles.
This class contains a dynamic array of points that are the vertices for the leader line. The first point in the array is the start of the leader. If the leader has an arrowhead it is located here. If the length of the first segment of the leader is less than twice the arrowhead size the arrowhead is suppressed. If the leader has an associated annotation object the last point of the... more  
 AcDbLight  This is a container for all properties relating to a generic light. A dictionary of these objects is resident in the database, in the named object dictionary as ACAD_LIGHTS. They are indexed by the name of the setting objects. In the user interface, the contents of this dictionary correspond to user-defined light properties (displayed on a property palette accessible by selection of the light using a variety of methods.  
 AcDbLine  The AcDbLine class represents the line entity in AutoCAD. A line object is a 3D object that is specified by its start point, endpoint, and normal vector. In addition, the line object supports thickness along its normal vector direction (that is, height or "extrusion").  
 AcDbLinetypeTable  This class is the symbol table for AcDbLinetypeTableRecords, which represent the linetypes within a drawing database.
  
 AcDbLinetypeTableIterator  This class is for iterating over the records in an AcDbLinetypeTable.
The only substantial difference from the base class AcDbSymbolTableIterator is the use of AcDbLinetypeTableRecord-specific signatures for the member functions getRecord() and seek().  
 AcDbLinetypeTableRecord  Objects of this class represent records in the AcDbLinetypeTable. Each of these records contains the information about a linetype in the drawing database.
Within the LinetypeTableRecord, the dashes (line segments that make up characteristics of the linetype) are stored in a list with an index that is zero based. If the linetype is complex, then embedded shapes or text strings are stored in the list at the same index as the dash that preceded them in the linetype definition. So there will always be a dashLength for any valid index in the list, even if there is a... more  
 AcDbLinkedData  This class is used to support data linking.
  
 AcDbLinkedTableData  This class is used to store and manage data in a tabular structure. Though this class is similar to a database table it differs in one respect: all the cells in a column need not be of the same data type. In addition, the cells in this table can contain multiple contents, and the contents can be a value of any supported data type, field, or block. This table stores and manages only data without any format information.
  
 AcDbLoftedSurface  This class represents a lofted surface entity. It provides methods to create and manipulate ASM surfaces.  
 AcDbLoftOptions  Utility class for setting options used by AcDbLoftedSurface::createLoftedSurface() . These options can be used to control the shape of the resulting surface. The default constructor for this class assigns default values to these options.
  
 AcDbLoftProfile  This class is used to represent a profile that can be used as input to createLoftedSurface(). An object of this class can represent an entity such as a curve or a region, or it can represent an edge or a set of edges that form a connected chain. This class also maintains continuity data if the profile is for an edge or point and the profile is the first or last profile of the loft.   
 AcDbLongTransaction  This is the class that contains the information needed to track a long transaction. The AcLongTransactionManager class takes the responsibility for creating and appending AcDbLongTransaction objects to the database. It then returns the AcDbObjectId of the object. Destruction of this object is handled by the database (like other database resident objects).  
 AcDbLongTransWorkSetIterator  Provides read-only access to the objects in the work set. During the construction of an AcDbLongTransWorkSetIterator object (by AcDbLongTransaction::newWorkSetIterator), it can be set to include only the active work set, or to also include objects which were added to the work set because they are referenced by objects in the work set (secondary objects), and/or objects which have been removed from the work set, either by AcDbLongTransaction::removeFromWorkSet(), or by being erased.  
 AcDbMatchProperties  The AcDbMatchProperties class is an abstract base class for a series of protocol extension classes that add the ability to copy properties between entities.
Derived classes for all the built-in entity types are provided as a part of the match.arx application. For custom classes that are not a part of AutoCAD, the custom class's application may derive its own class from the AcDbMatchProperties base class (and hook an object of the class into the run-time tree in the normal manner for protocol extension) in order to tie into the existing match properties interface that comes with AutoCAD (in match.arx).... more  
 AcDbMaterial  The AcDbMaterial class is used to define, store, and display material properties for shaded entities. An AcDbMaterial object consists of a variety of shading components, including diffuse, ambient, specular, reflection, opacity, bump, and refraction. Each component defines a different aspect of shading behavior for an entity.
AcDbMaterial objects are stored in the ACAD_MATERIAL dictionary within the named object dictionary of an AcDbDatabase.
  
 AcDbMentalRayRenderSettings  This class is a container for all properties relating to the Mental Ray renderer. 
  
 AcDbMInsertBlock  The AcDbMInsertBlock class represents a special form of the INSERT entity in which an array of duplicate images of the entities within the referenced AcDbBlockTableRecord is displayed.
Creating an AcDbMInsertBlock with a definition of rows = 1 and columns = 1 will create a AcDbBlockReference instead.  
 AcDbMLeader  The AcDbMLeader class represents the multi-leader object.  
 AcDbMLeaderStyle  This class is the main class used for the multiple leader style object.
  
 AcDbMline  The AcDbMline class represents the AutoCAD MLINE entity. This multi-line entity allows the user to create a complex line with multiple parallel line parts, each with its own linetype. The space between these parallel lines can be filled if desired.  
 AcDbMlineStyle  AcDbMlineStyle class objects are used to store the information about the number, linetypes, and offsets of multi-line line patterns to be used by AcDbMline entities.
An mline style consists of a series of elements, each of which is a line at a given offset from an axis (there may or may not be an element on the axis).
AcDbMlineStyle objects are stored in the ACAD_MLINESTYLE dictionary within the Named Object Dictionary of AcDbDatabase.  
 AcDbModelDocViewStyle  The abstract base class for the AcDbSectionViewStyle and AcDbDetailViewStyle classes.  
 AcDbMotionPath  This class associates a motion path with a camera and viewport. You can use the motion path to create animations and walk throughs.
  
 AcDbMPolygon  Handles one or more closed polygons as a unit. Contains an AcDbHatch member for rendering and other tasks, via AcDbMpolygon::hatch( ).
  
 AcDbMText  The AcDbMText class is the representation for the MTEXT (multiline text) entity within AutoCAD.  
 AcDbMultiModesGripPE  When implemented and registered as a protocol extension to an AcDbEntity-derived class, this protocol extension interface specifies the communication with the grip editing complex for information on what alternatives can be provided to stretching grip points.
These alternatives are represented as an array of modes (of type AcDbMultiModesGripPE::GripMode).
The grip editing complex is responsible for querying all available modes, interacting with the user to set the current mode, or switching between modes, and either: 
proceeding with grip dragging (if the current mode's action type is kDragOn), 
calling moveGripPointsAt once (if the current mode's action type is kImmediate) 
executing a command... more 
 
 AcDbNamedPath  This class is used to associate a named path with a camera and viewport for creating animations.
  
 AcDbNamedObjectDictionaryPointer  This class allows you to access the AcDbNamedObjectDictionary object of a given database.
The following pre-defined typedefs are available: 
AcDbDictionaryPointer 
AcDbEntityPointer 
These classes are designed to be type-safe replacements for explicitly using acdbOpenObject(), acdbOpenAcDbObject(), and acdbOpenAcDbEntity(). Using these classes incurs opening and closing objects, which, under certain circumstances, may be better managed using transactions.  
 AcDbNonSubEntDynamicUCSPE  This is version 2 of DUCS Protocol extension abstract base class. Supports: 
Planes on non sub-entities 
Owner draw of plane hightlighting 
Currently, point cloud object uses this.  
 AcDbNurbSurface  This class implements the AutoCAD NURBS surface entity. Objects of the AcDbNurbSurface class use ASM (Autodesk shape manager) which is a solid modeler. The following books are a good place to start to get a basic understanding of spline curves: Curves and Surfaces for CAGD by Gerald Farin The essentials of CAGD by Gerald Farin   
 AcDbObject  The AcDbObject class is the base class for all objects that reside in an AcDbDatabase object. This class provides all the functionality for database residency such as objectId, handle, ownership backpointer, filing, persistent reactor notification, xdata, deepClone, object state queries, erase, audit, etc. Many of these involve virtual functions so that they can be overridden in classes derived from AcDbObject.  
 AcDbObjectContext  An abstract base class for representing a particular context which may affect the properties and/or behavior of one or more types of objects.Classes that are derived from this base class are used to represent a particular type of context such as annotation scale.  
 AcDbObjectContextCollection  An interface to an application-defined collection of AcDbObjectContext objects. 
Applications implement this interface to define custom object contexts for their context-sensitive objects. Applications must instantiate an instance of the collection (typically one instance for each database) and register it with the AcDbObjectContextCollectionManager for each AcDbDatabase which needs to support the context type. Each context collection has a state; this is represented by the collection of contexts, the "current" context, and the ability to lock or unlock context instances. 
  
 AcDbObjectContextCollectionIterator  An iterator interface for AcDbObjectContext objects stored in an AcDbObjectContextCollection. 
Callers obtain an iterator by calling AcDbObjectContextCollection::newIterator(). Applications that provide custom AcDbObjectContextCollection implementations must also implement this interface in a custom iterator for their collection. 
  
 AcDbObjectContextInterface  Abstract protocol extension interface class which allows supporting context-specific behavior on an object. 
Custom objects provide an implementation of this interface and associate it with their AcRxClass to provide context-dependent object behavior. One example of such behavior is where the context is the current viewport scale, and where the custom object behavior for text is the text height, orientation, and position. Applications can define other context types (AcDbObjectContext custom classes) and store them in custom collections (AcDbObjectContextCollection). By implementing this interface, custom objects expose information about which context types they support and when context instances they participate... more  
 AcDbObjectContextManager  Exposes the collection of context types supported by the drawing. 
Each AcDbDatabase exposes an AcDbObjectContextManager which can be used to register, unregister, and enumerate custom context collections. 
  
 AcDbObjectId  Since the Release of AutoCAD R13, there has been a mechanism for dealing with database-resident objects in memory. The scheme has two parts for each object. First there is the database-resident object itself, which resides in memory and can be paged out to disk if memory needs to be freed up. The second part is a "stub" object (class AcDbStub) that always resides in memory and acts as the access point for the database-resident object.
When an object or entity is first added to the database, a new stub object is created and set to point to the object... more  
 AcDbObjectIdGraph  AcDbObjectIdGraph is derived from AcDbGraph and is used to represent the relationship of object IDs to one another. This class is used by the one pass purge method (AcDbDatabase::purge(AcDbObjectIdGraph& idGraph)) to represent the references from one object ID to another.
  
 AcDbObjectIdGraphNode  AcDbObjectIdGraphNode is derived from AcDbGraphNode and is used for representing an object ID in a database.
  
 AcDbObjectIterator  Objects of this class are used for iterating through the objects contained in the complex objects AcDbBlockReference, AcDb2dPolyline, AcDb3dPolyline, AcDbPolyFaceMesh or AcDbPolygonMesh. In all but the AcDbBlockReference case, the objects being iterated over are vertices.
The class constructor is private so instances of this class cannot be instantiated directly. To create an object of this class, you must use the iterator creation functions provided within the classes the iterator can iterate through.  
 AcDbObjectOverrule  AcDbObjectOverrule overrules a subset of operations that AcDbObject class specifies. It is intended as a base class for clients who want to alter some or all behavior of a given AcDbObject-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcDbObjectReactor  The AcDbObjectReactor class is used as the base class for reactor classes that override methods called for events for which notification is desired. When an object of one of these derived classes is instantiated and added to the reactor chain of the object for which notification is desired, then whenever the "notifying" object is accessed, the appropriate methods within the reactor object will be called.  
 AcDbOle2Frame  This class provides support for OLE 2 objects in AutoCAD drawings. Essentially it provides a window to display the OLE 2 object it has a pointer to.
  
 AcDbOleFrame  This class provides support for pre-R13c4 OLE 1 retrieval only. It exists only to allow reading in old pre-R13c4 drawings that contained OLE 1 objects. When such objects are read in, the user is asked if she'd like to convert them to OLE 2, at which point they are converted to AcDbOle2Frame objects. All new OLE objects inserted, pasted, or dragged-and-dropped are AcDbOle2Frame.  
 AcDbOrdinateDimension  The AcDbOrdinateDimension class represents the ordinate dimension type within AutoCAD.
Ordinate dimensions measure the "horizontal" (X axis) or "vertical" (Y axis) distance from a specified origin point to some other specified point.
AcDbOrdinateDimensions measure the distance from the their origin point to their definingPoint along the X or Y axis (as specified by the appropriate member function). They display a leader line from the definingPoint to the leaderEndPoint, with the annotation text located appropriately near the end of the leader.  
 AcDbOsnapOverrule  AcDbOsnapOverrule overrules a subset of osnap related operations that the AcDbEntity class specifies. It is intended as a base class for clients who want to alter some or all behavior of a given AcDbEntity-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcDbOsnapPointRef  
This class is used to capture the Osnap based reference between a dimension and the geometry it is associated with.  
 AcDbPaperOrientationPE  This protocol extension interface can optionally be implemented by objects which support "paper orientation" behavior. 
  
 AcDbParentTransformOfChildPE  Protocol extension used by AcDbCompoundObjectId::getTransform() to find the transform that the parent object applies to its child object, if the AcDbCompoundObjectId contains a whole path of objects. For example for AcDbBlockReference the returned transform is AcDbBlockReference::blockTransform(). If the path contains other parent objects than AcDbBlockReference (it seems to be allowed, e.g. the parent object may be a custom entity), the parent object may specify the transform. If the parent object does not expose this PE, an identify transform is assumed.  
 AcDbPasswordedFile  Derived classes are expected to override tryPassword and cache a file moniker (file name, file handle etc.) during construction. so they can try the passwords passed to tryPassword. Here are the possible conditions for wszPassword:
null - check the password cache first then prompt for password
"some-value" -- try the supplied value then the cache but don't prompt for another password if it fails
"" -- skip the cache and the prompt and just fail if encrypted  
 AcDbPathRef  Reference to a path, which is just a sequence of edges. If the path has an array of edges which happen to be connected at their end points, it will try to concatenate the curves into one composite curve.   
 AcDbPdfDefinition  Represents PDF underlays in the drawing. Underlays are similar to raster images but their content is snappable.
.  
 AcDbPdfReference  
Represents PDF underlays in the drawing. Underlays are similar to raster images but their content is snappable.  
 AcDbPlaceHolder  This class provides a way to instantiate what is essentially just an AcDbObject. This class has no data members of its own, nor does it implement any new methods. Its sole intent is to provide objects that can be added to dictionaries so that the dictionary lookup key strings can be associated with object IDs.
Objects of this class are used in the Plot Style Name Dictionary.
  
 AcDbPlaneSurface  This class represents a plane surface entity. It provides methods to create and manipulate ASM surfaces.
  
 AcDbPlotSettings  The AcDbPlotSettings object stores "what-to-plot" settings as a named plot setup description. The plot settings objects are stored in an AcDbPlotSettingsDictionary.  
 AcDbPlotSettingsValidator  This class exports access methods that validate data on an AcDbPlotSettings object before actually setting the data on the object.  
 AcDbPoint  The AcDbPoint class represents the point entity within AutoCAD. A point entity has a position value to locate it in space. It also has a normal vector, which is used to determine its extrusion direction and to determine the plane on which to display the point's graphics if they are something other than a "." (the PDMODE system variable controls this display). The point entity has a thickness that is its "length" in the direction of its normal vector (that is, its extrusion length).  
 AcDbPointCloudClassificationColorRamp  The AcDbPointCloudClassificationColorRamp object is the color ramp instance which represents a classification color ramp   
 AcDbPointCloudColorMap  The AcDbPointCloudColorMap class stores all color map ramp definitions. There is only one AcDbPointCloudColorMap object per database. The user can use getColorMap() to retrieve the AcDbPointCloudColorMap object. There are two kinds of color ramps: 
intensity or elevation color ramps (which are used when the point cloud is stylized by intensity or elevation) 
classification color ramps (which are used when the point cloud is stylized by classification). 
The color ramps are indexed by GUIDs inside the AcDbPointCloudColorMap object and point clouds reference these ramps by GUIDs.  
 AcDbPointCloudColorRamp  The AcDbPointCloudColorRamp object is the color ramp instance which represents an intensity color ramp or elevation color ramp   
 AcDbPointCloudCrop  This class defines a cropping boundary for the AcDbPointCloudEx entity. Each cropping boundary contains a plane which locates the boundary, a cropping type, a flag indicating whether we want what is inside or outside the region, another flag indicating whether the results of this cropping boundary should be inverted.  
 AcDbPointCloudCropStateManager  This class includes all the crop states functions for point cloud. From the class you can get the saved crop state names, save current crops, delete a crop state, rename a crop state and get the current active crop state. The crop state manager is per point cloud object, you can get its pointer from AcDbPointCloudEx::cropStatesManager() or AcDbPointCloudEx::cropStatesManagerConst().  
 AcDbPointCloudDefEx  The AcDbPointCloudDefEx object (or "point cloud extension definition object") controls the actual point cloud data associated with one or more AcDbPointCloudEx entities (or "point cloud extension entity"). These objects link the .rcp/.rcs file to the dwg file, similar to how Xrefs work.
The relationship between the AcDbPointCloudDefEx and AcDbPointCloudEx classes is much like the relationship between an AutoCAD block definition object and a block reference entity.
AcDbPointCloudDefEx objects are stored in a special AcDbDictionary named "ACAD_POINTCLOUD_EX_DICT". These objects are not AutoCAD entities, so they can't be displayed or selected by the end user.   
 AcDbPointCloudDefReactorEx  The AcDbPointCloudDefReactorEx class is used to notify AcDbPointCloudEx entities of relevant modifications to their associated AcDbPointCloudDefEx object. It is a persistent reactor of AcDbPointCloudDefEx object to connect AcDbPointCloudEx entities.
Specifically, modification of the AcDbPointCloudDefEx object triggers the redraw of each of its dependent AcDbPointCloudEx entities. Erasing the AcDbPointCloudDefEx object triggers the erasing of its dependent AcDbPointCloudEx entities.  
 AcDbPointCloudEx  The AcDbPointCloudEx entity (or "point cloud extension entity") works with the AcDbPointCloudDefEx object (or "point cloud extension definition object") to manage point cloud data inside AutoCAD. The relationship between these two classes is much like the relationship between an AutoCAD block definition object and a block reference entity.
The AcDbPointCloudEx entity is a drawable, selectable AutoCAD entity that places point cloud data in model or paper space at a particular location and orientation, and with a particular scale. It also contains crop boundaries, scan and region visibilities, stylization types, color mapping schemes, a limit box, and other typical AcDbEntity properties,... more  
 AcDbPointPath  This class defines a point path.
  
 AcDbPointRef  This class is used to capture the point based reference between a dimension and the geometry it is associated with.  
 AcDbPolyFaceMesh  The AcDbPolyFaceMesh class represents the PolyFaceMesh special polyline entity type within AutoCAD.  
 AcDbPolyFaceMeshVertex  The AcDbPolyFaceMeshVertex class represents the vertices within AcDbPolyFaceMesh entities in AutoCAD drawings.  
 AcDbPolygonMesh  The AcDbPolygonMesh class represents the polygon mesh entity within AutoCAD.
A PolygonMesh is an M x N mesh, where M represents the number of vertices in a row of the mesh and N represents the number of vertices in a column of the mesh.
A mesh can be open or closed in either or both the M and N directions. A mesh that is closed in a given direction is considered to be continuous from the last row or column on to the first row or column.
All the vertices in the mesh are stored in a single list. For... more  
 AcDbPolygonMeshVertex  The AcDbPolygonMeshVertex class represents vertices within polygon meshes in AutoCAD.  
 AcDbPolyline  AcDbPolyline is often called a lightweight polyline because of its efficient use of memory. AcDbPolyline provides greater performance and reduced overhead compared to AcDb2dPolyline. Its database representation is more efficient because it is stored as a single object with a single name, handle, type, space, layer, and width.

A lightweight polyline has: 
Straight line segments 
Bulge (arc segments) 
Constant and variable width 
Thickness 

Functionality and command options not available in a lightweight polyline include: 
Arc Fit curve data 
Spline Fit data 
Curve fit tangent direction data 

The lightweight polyline is created with the PLINE command and edited with the... more  
 AcDbPropertiesOverrule  AcDbPropertiesOverrule overrules a subset of property related operations that the AcDbEntity and AcDbObject classes specify. It is intended as a base class for clients who want to alter some or all behavior of a given AcDbEntity-derived or AcDbObject-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcDbPropertyFilterCustomizationPE  
This protocol extension interface can be implemented and associated with an AcRxClass to take some level of control over the way an entity type participates in property filter customization in the CUI dialog. If this protocol extension is not associated with the AcRxClass of a particular entity type, the entity type will be handled in a "default" manner.

The "default" behavior is as follows: the CUI property filter customization framework will make the entity type available for property filter customization when it is registered in the class dictionary by instantiating an instance of the entity via the AcRxClass::create() method. It... more  
 AcDbProxyEntity  This abstract class provides an interface to the description data for the entities stored within ProxyEntities in drawings. It also provides access to an explode method that results in a set of entities that are copies of the graphics metafile data stored in the proxy.  
 AcDbProxyObject  This abstract class provides an interface to the description data for the objects stored within ProxyObjects in drawings.  
 AcDbRadialDimension  The AcDbRadialDimension class represents the radius dimension type in AutoCAD.
This dimension type requires a center point and a point on the curve being dimensioned in order to be able to draw the dimension line from the center point through the point on the curve. In addition, it utilizes a "leader length" value to determine how far the dimension line extends out past the curve before doing a horizontal dogleg (if necessary) to the annotation text.  
 AcDbRadialDimensionLarge  This class represents a large radial dimension, also known as a jogged radius dimension.
  
 AcDbRapidRTRenderSettings  Container for all properties relating to the rapid RT renderer. See the base class AcDbRenderSettings for more information.   
 AcDbRasterImage  The AcDbRasterImage entity (or "image entity") works with the AcDbRasterImageDef object (or "image definition object") to implement raster images inside AutoCAD. The relationship between these two classes is much like the relationship between an AutoCAD block definition object and a block insert entity.
The image entity is a drawable, selectable AutoCAD entity that places a raster image in model or paper space at a particular location and orientation. The image entity is linked to exactly one image definition object, to which it sends requests for image processing operations needed for displaying and plotting images. Because the image definition object manages... more  
 AcDbRasterImageDef  The AcDbRasterImageDef object (or "image definition object") works with the AcDbRasterImage entity (or "image entity") to implement raster images inside AutoCAD. The relationship between these two classes is much like the relationship between an AutoCAD block definition object and a block insert entity.
The image definition object plays a behind-the-scenes role like the block definition, maintaining links to the source image file and managing low-level image processing operations required to display and plot images. Image definition objects are stored in a special AcDbDictionary named ISM_RASTER_IMAGE_DICT. These objects are not AutoCAD entities, so they can't be displayed or selected directly... more  
 AcDbRasterImageDefFileAccessReactor  This is an abstract base-class defining an interface to a specialized non-persistent object reactor. Concrete classes derived from this class should be attached to the ISM image dictionary object, and will be notified of file related activity as described below.  
 AcDbRasterImageDefReactor  The AcDbRasterImageDefReactor object is used to notify AcDbRasterImage entities of relevant modifications to their associated AcDbRasterImageDef object.
Specifically, modification of the AcDbRasterImageDef object triggers the redraw of each of its dependent AcDbRasterImage entities. Erasing the AcDbRasterImageDef object triggers the erasing of each of its dependent AcDbRasterImage entities.
Linking to acISMobj17.lib
Class AcDbRasterImageDefReactor is implemented in an ObjectARX application called acISMui.arx. Your application must link to the ObjectARX API library acISMobj17.lib to use any of the methods specific to this class.  
 AcDbRasterImageDefTransReactor  The AcDbRasterImageDefTransReactor is a non-persistent version of AcDbRasterImageDefReactor. That is, it is not stored in the drawing database. It is used to notify applications or other entities of modifications to AcDbRasterImageDef objects. This is a pure virtual class. It must be subclassed and implemented before it can be created and attached to an AcDbRasterImageDef object.
AcDbRasterImageDefTransReactor supports the following AcDbRasterImageDefReactor notifications: erased, modified, and onDeleteImage. (Please see the AcDbRasterImageDefReactor class documentation for these methods.) However, there is no built-in setEnable() method for this class.  
 AcDbRasterVariables  The AcDbRasterVariables object carries all drawing-scope persistent variables associated with AutoCAD raster image management and display. A single object of this type is stored in each drawing file that contains image entities.  
 AcDbRay  The AcDbRay class represents the RAY entity type within AutoCAD.
A ray entity is a line that has a start point, but continues on to infinity in some direction.  
 AcDbRecover  This class has not been fully implemented. It is reserved for possible future use.
  
 AcDbRecoverCallBack  This class has not been fully implemented. It is reserved for possible future use.
  
 AcDbRegAppTable  This class is the symbol table for AcDbRegAppTableRecords, which represent registered application names for Extended Entity Data within objects that reside in the drawing database.  
 AcDbRegAppTableIterator  This class is for iterating over the records in an AcDbRegAppTable.
The only substantial difference from the base class AcDbSymbolTableIterator is the use of AcDbRegAppTableRecord-specific signatures for the member functions getRecord() and seek().  
 AcDbRegAppTableRecord  Objects of this class represent records in the AcDbRegAppTable (known as the APPID symbol table in AutoCAD and DXF). Each of these records represents an application ID used to identify a group of Extended Entity Data attached to objects in the drawing database.
  
 AcDbRegion  Objects of this class represent region entities in AutoCAD.
An AcDbRegion entity is a container and interface for a ShapeManager object that is the actual geometrical representation of the region. This class provides no provisions for directly manipulating the edges, vertices, or faces of the ShapeManager object that is the actual region representation.  
 AcDbRenderEnvironment  This is a container class for environment-related properties, including fog / depth cue and the global environment image. Only one object of this type is maintained by the per-drawing AcDbRenderGlobal object, at ACAD_RENDER_ENVIRONMENT in the named object dictionary.  
 AcDbRenderGlobal  Container for all global rendering properties. One and only one of these objects is resident in the database, in the named object dictionary as ACAD_RENDER_GLOBAL.  
 AcDbRenderSettings    
 AcDbRevolvedSurface  This class represents a revolved surface entity. It provides methods to create and manipulate ASM surfaces.  
 AcDbRevolveOptions  Utility class for setting options used by createRevolvedSurface() and createRevolvedSolid(). These options can be used to control the shape of the resulting surface or solid. The default constructor for this class assigns default values to these options.  
 AcDbRotatedDimension  The AcDbRotatedDimension class represents the dimension type that dimensions the distance between two points in space when they are projected onto a line at a specific angle (the rotation angle) within the dimension's plane.
A "horizontal" dimension is a rotated dimension with an angle equal to the angle between the dimension's OCS X axis and the X axis of the UCS used to define "horizontal." A "vertical" dimension is a rotated dimension with an angle equal to pi / 2 radians (90 degrees) greater than the angle for a "horizontal" dimension.  
 AcDbSection  This class represents the section plane entity in AutoCAD.  
 AcDbSectionablePE  AcDbSectionablePE Protocol Extension  
 AcDbSectionManager  This class is used to manage the section planes in a database. This class cannot be instantiated. An object of this class can be obtained form the database using the AcDbDatabase::getSectionManager() method.
  
 AcDbSectionManagerIterator  This class is used to iterate the section planes in a database.
  
 AcDbSectionSettings  This class stores section geometry settings.
  
 AcDbSectionSymbol  The AcDbSectionSymbol class representing the section symbol entity created by the model documentation feature.  
 AcDbSectionViewStyle  The style object for a section view.  
 AcDbSelectGrip  This class allows applications to determine whether a grip on a custom entity is selectable.  
 AcDbSelectGrip2  This class allows applications to determine whether a grip on a custom entity is selectable.  
 AcDbSequenceEnd  The AcDbSequenceEnd class represents the SEQEND entity within AutoCAD. Objects of this class are created and handled automatically by AutoCAD for each complex entity that requires them, such as AcDbBlockReference, AcDb2dPolyline, AcDb3dPolyline, etc.
Applications do not need to create objects of this class, nor do they need to add or remove them from databases. Applications are free to add or manipulate xdata on objects of this class, as well as work with extension dictionaries for objects of this class.  
 AcDbShape  The AcDbShape class represents the SHAPE entity within AutoCAD.  
 AcDbSkyBackground  This class represents a sky background.
  
 AcDbSoftOwnershipId  This class is a specialization of AcDbObjectId that is used to distinguish an object specified by an AcDbSoftOwnershipId as being soft owned by some other object. Other properties are the same as the more generic AcDbObjectId.  
 AcDbSoftPointerId  This class is a specialization of AcDbObjectId that is used to distinguish an object specified by an AcDbSoftPointerId as being soft pointed to by some other object. Its other properties are the same as the more generic AcDbObjectId.  
 AcDbSolid  The AcDbSolid class is the representation for the SOLID entity within AutoCAD. The SOLID entity is a 2D entity, not the 3D solids that are represented by the AcDb3dSolid and AcDbBody classes.  
 AcDbSolidBackground  This class represents a solid background.
  
 AcDbSortentsTable  AcDbSortentsTable is the persistent container for draw order information. It resides in the extension dictionary of an associated AcDbBlockTableRecord under the key ACAD_SORTENTS.
AcDbSortentsTable contains a set of object ID/handle pairs. The object ID is that of the entity to be drawn, and the handle is that of an entity in the block table record (usually but not always different from the associated object ID), which represents the position in the draw order. Entities are appended to a block table record in order of ascending handle value; in other words, the higher the handle value, the later it appears... more  
 AcDbSpatialFilter  This class defines an extruded volume based on a 2D boundary definition, local coordinate system, and the front and back clipping planes.
  
 AcDbSpatialIndex  This class is an index implementation especially for the spatial index.  
 AcDbSpatialIndexIterator  This class is used by the AcDbSpatialIndex::newIterator() method, and it iterates on entities that satisfy a spatial query defined by the AcDbSpatialFilter object.  
 AcDbSpline  This class implements the AutoCAD SPLINE entity.
Objects of the AcDbSpline class use an embedded gelib object to maintain the actual spline information. The spline itself may either be a simple curve fit (within a specified tolerance which may be 0) through a set of "fit points," or it may be a NURBS spline (that is, a set of control points, knots, and weights used to define the spline path). Internally, a curve-fit spline still has NURBS data; however the reverse is not true.
The following books are a good place to start to get a basic understanding of spline... more  
 AcDbSubDMesh  The AcDbSubDMesh class represents the SubDivision surface entity type within AutoCAD.  
 AcDbSubentGeometry  This class describes the subentity geometry and its subentity type.  
 AcDbSubentId  An entity can be composed of many graphic subentities, such as arcs, lines, text, vertices, edges, faces, etc. This class creates subentity IDs using one of these subentities and an index. The index is the graphics system marker for the subentity. The SubentType can be one of the following:
AcDb::kNullSubentType (used as a NULL)
AcDb::kFaceSubentType
AcDb::kEdgeSubentType (lines, arcs, etc.)
AcDb::kVertexSubentType
AcDb::kMlineSubentCache  
 AcDbSubentityOverrule  AcDbSubentityOverrule overrules a subset of subentity related operations that implemented in AcDbEntity. It is intended as a base class for clients who want to alter some or all behavior of a given AcDbEntity-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcDbSubentRef  <para xmlns="http://buildtree.tv/SpecSchema.xsd"> Base class for AcDbFaceRef, AcDbEdgeRef and AcDbVertexRef. It keeps an AcDbCompoundObjectId of an AcDbEntity and AcDbSubentId of a subentity of this entity. </para>   
 AcDbSummaryInfoManager  The AcDbSummaryInfoManager class organizes the summaryInfoHasChanged reactors.  
 AcDbSummaryInfoReactor  This class provides a reactor to let you know if the Summary Info is changed.  
 AcDbSun  Container for all properties relating to a sun object. A sun exists as a hard-owned child of an AcDbViewportTableRecord or an AcDbViewport object. It is created when a sun-related operation is first performed (for example, changing a property on the Sun Properties Dialog or a sun-related parameter in the dashboard). It is also created when viewports are created by the sun study wizard. It is set and accessed by the APIs setSun() and sunId() defined on both viewport classes mentioned above. The properties defined in this class correspond to fields on the Sun property palette, but are also set and... more  
 AcDbSurface  Surface entity with methods to create and manipulate ASM surfaces. The following classes derive from AcDbSurface:

AcDbExtrudedSurface 
AcDbLoftedSurface 
AcDbPlaneSurface 
AcDbRevolvedSurface 
AcDbSweptSurface 
  
 AcDbSurfaceTrimInfo  AcDbSurfaceTrimInfo describes how a (blank) surface will be trimmed: the information about trimming (tool) surface and trimming curve (if it is a trimming curve, a projection vector should be provided so the curve will be projected on to the blank surface). Also necessary is the topological information between the trimmed (blank) surface and trimming(tool) surface/curve - if the area intended to be trimmed is inside or outside of the trimming curve/surface   
 AcDbSweepOptions  Utility class for setting options used by createSweptSurface() and createSweptSolid(). These options can be used to control the shape of the resulting surface or solid. The default constructor for this class assigns default values to these options.
  
 AcDbSweptSurface  This class represents a swept surface entity. It provides methods to create and manipulate ASM surfaces.
  
 AcDbSymbolTable  AcDbSymbolTable is the base class for all of the classes used to export AutoCAD's built-in symbol tables.
  
 AcDbSymbolTableIterator  This class is a base class for iterator classes that are used for iterating over the records in the AcDbSymbolTable objects.  
 AcDbSymbolTableRecord  This is the base class for each type of record corresponding to the various symbol tables.  
 AcDbTable  AcDbTable is a planar entity that can be created and placed in an arbitrary plane in 3D space. A table is generally thought of as an n x m rectangular array of cells whose contents consist of annotation objects, primarily text. In the AEC industry, tables are often referred to as "schedules" and contain information on the materials needed for construction of the building being designed. In the manufacturing industry, tables are referred to as "BOMs" (bills of materials); in GIS, they are referred to simply as "tables." Tables often contain a title row, a header row, and multiple data... more  
 AcDbTableContent  This class derives from AcDbFormattedTableData and supports formatting of table using table style and cell styles.
  
 AcDbTableIterator  This class creates an iterator to traverse table rows, columns, and cells.
  
 AcDbTableStyle  AcDbTableStyle class objects are used to store the table formatting properties (for example, grid visibility, lineweight, line color, and cell font, height, color, and background fill) used by AcDbTable entities. 
AcDbTableStyle objects are stored in the ACAD_TABLESTYLE dictionary within the named object dictionary of AcDbDatabase.
Because tables appear in a variety of forms, table objects will be based on a table style similar to the way text objects and dimension objects are based on styles in AutoCAD. The table style controls the initial formatting of a newly created table object and whether the table includes a title and header... more  
 AcDbTableTemplate  This class is used to capture, store, and manage templates of AutoCAD tables.
  
 AcDbText  The AcDbText class is the representation for the TEXT entity within AutoCAD.  
 AcDbTextEditor  The AcDbTextEditor represent the text editor in the db. You can create a text editor from a AcDbMtext entity. From the text editor the user can access the information about the MTEXT objects like selection, cursor, columns, paragraphs, wipeout and so on. You can also input/delete some characters in the current cursor position (AcDbTextEditorCursor). When you don't need the text editor, make sure to call the close() function to release the allocated resources. In that function it has a parameter to tell the editor to save/don't save the changes back to the MTEXT object. Also please note that the text... more  
 AcDbTextEditorColumn  AcDbTextEditorColumn repersents the column object in text editor. All the columns in editor can have different height value but they have the same width. If you don't need it, call its release() function. Otherwise, it will cause a memory leak.  
 AcDbTextEditorColumns  This class keeps the common properties for all the column objects in text editor. The user can get the object from the AcDbTextEditor::columns() function. Do not release the object. Its life cycle is managed by the core.  
 AcDbTextEditorCursor  The AcDbTextEditorCursor class represents the current cursor object in text editor. The user can change the text properties and type text at the current cursor (if you typed new text at the cursor it will have the new properties you set).
The user can move the cursor. Please note that the current cursor and the current selection object are mutually exclusive. If there is a valid selection, there is a NULL cursor object. Otherwise, if there is a valid cursor object, the editor has an empty selection.  
 AcDbTextEditorLocation  AcDbTextEditorLocation represents information about the location in the core text editor. When you have the object, you can call the functions in the AcDbTextEditor like makeSelection(), findText() and so on. Note that the location in the text editor is volatile object. If you saved one location, then call some some functions (for example the columns setting functions, paragraph setting function, selection/cursor inserting functions and so on.) that cause the text editor re-wrap itself, the saved location is invalid.  
 AcDbTextEditorParagraph  AcDbTextEditorParagraph represents the paragraph object in the editor. When it is no longer needed. call its release function. Otherwise, it will be a source of a memory leak. If the current text editor defined width(AcDbTextEditor::definedWidth()) is zero or the current text editor text style is vertical SHX, it can't support the properties in paragraph like indent, space, alignment, tabs and so on. Calling those functions will return Acad::eNotApplicable.  
 AcDbTextEditorParagraphIterator  This is the paragraph iterator in text editor.   
 AcDbTextEditorParagraphTab  AcDbTextEditorParagraphTab represents the tab object in the editor. You can get and set paragraph tab object properties. Its constructor creates a tab object with default properties.  
 AcDbTextEditorSelectable  AcDbTextEditorSelectable is a pure interface that represents the locations for the start and end position of selectable objects in the text editor.  
 AcDbTextEditorSelection  AcDbTextEditorSelection represents the current selection object in the text editor.This class allows you to get and set the selected stack and change stack properties.
Note that after AcDbTextEditor::clearSelection() has been called, the text editor doesn't have a valid selection; calling other class methods will produce an error code indicating that the selection is empty. A valid selection is achieved with AcDbTextEditor::makeSelection() or AcDbTextEditor::selectAll().  
 AcDbTextEditorSelectionBase  AcDbTextEditorSelectionBase is the base class for the AcDbTextEditorCursor and AcDbTextEditorSelection. I
It includes all the common methods for the two classes. Calls via AcDbTextEditorSelection objects changes properties for the whole selection. Calls via AcDbTextEditorCursor changes properties for all text following the current cursor.  
 AcDbTextEditorStack  AcDbTextEditorStack represents stack object properties in the text editor. The stack object manages text like fractions.
This class allows you to get and set the properties for the selected stack, for example, changing the top text, the bottom text or the stack type.  
 AcDbTextEditorWipeout  AcDbTextEditorWipeout represents the wipe out object in the editor. You can get the object from AcDbTextEditor::wipeout(). This object type should not be released; its life cycle is managed by the core.  
 AcDbTextStyleTable  This class is the symbol table for AcDbTextStyleTableRecords which represent text styles.  
 AcDbTextStyleTableIterator  This class is for iterating over the records in an AcDbTextStyleTable.
The only substantial difference from the base class AcDbSymbolTableIterator is the use of AcDbTextStyleTableRecord-specific signatures for the member functions getRecord() and seek().  
 AcDbTextStyleTableRecord  Objects of this class represent the records that are found in the AcDbTextStyleTable (known as the "Style" table in DXF). Each of these records represents a specific set of text parameters such as font, default size, relative x scaling, vertical or horizontal, etc.  
 AcDbTrace  The AcDbTrace class is the representation for the TRACE entity within AutoCAD.  
 AcDbTransactionManager  There is a single object of class AcDbTransactionManager created by the ObjectARX system when AutoCAD first starts up. This AcDbTransactionManager object is globally available to all ObjectARX applications.

The system AcDbTransactionManager object is used to start, end, or terminate transactions. In addition, it provides functionality to manage transactions and the objects within them.

For more information on the transaction mechanism, see the transaction section in the ObjectARX Developer's Guide.  
 AcDbTransformOverrule  AcDbTransformOverrule overrules a subset of transform related operations that the AcDbEntity class specifies. It is intended as a base class for clients who want to alter some or all behavior of a given AcDbEntity-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcDbUCSTable  This class is the symbol table for AcDbUCSTableRecord, which represents stored user coordinate systems (UCS) within the drawing database.  
 AcDbUCSTableIterator  This class is for iterating over the records in an AcDbUCSTable.
The only substantial difference from the base class AcDbSymbolTableIterator is the use of AcDbUCSTableRecord-specific signatures for the member functions getRecord() and seek().  
 AcDbUCSTableRecord  Objects of this class represent records in the AcDbUCSTable. Each of these records contains the information about a user coordinate system (UCS) that has been saved in the drawing database.  
 AcDbUnderlayDefinition  This abstract class handles the linkage to external underlay content. Underlays are similar to raster images, except that users can snap to the content of an underlay.
An AcDbUnderlayDefinition object is referenced by zero or more AcDbUnderlayReferences. The AcDbUnderlayReference class is responsible for the placement of content within the drawing, while the AcDbUnderlayDefinition class handles the linkage to the underlay content. Instances of AcDbUnderlayReference-derived concrete classes are inserted in a block table record. Instances of AcDbUnderlayDefinition-derived concrete classes are inserted into a dictionary within the named object dictionary.  
 AcDbUnderlayFile  This interface is implemented by host applications that wish to support underlays. AcDbUnderlayDefinition and AcDbUnderlayReference objects use this interface to accomplish their mission.
  
 AcDbUnderlayHost  This interface is implemented by host applications that wish to support underlays. AcDbUnderlayDefinition and AcDbUnderlayReference objects use this interface to accomplish their mission. 
  
 AcDbUnderlayItem  This interface is implemented by host applications that wish to support underlays. AcDbUnderlayDefinition and AcDbUnderlayReference objects use this interface to accomplish their mission. 
  
 AcDbUnderlayReference  Abstract class that represents underlays in the drawing. Underlays are similar to raster images, but their content is snappable. An AcDbUnderlayReference object must reference a compatible AcDbUnderlayDefinition object.
The AcDbUnderlayReference object is responsible for the placement of the content within the drawing, while the AcDbUnderlayDefinition object handles the linkage to the underlay content. Instances of AcDbUnderlayReference-derived concrete classes are inserted into a block table record. Instances of AcDbUnderlayDefinition-derived concrete classes are inserted into a dictionary under the named object dictionary.  
 AcDbVertex  The AcDbVertex class is the base class for all the vertex types used by the AcDb2dPolyline, AcDb3dPolyline, AcDbPolyFaceMesh, and AcDbPolygonMesh classes within AutoCAD.
This class provides no methods other than a default constructor and destructor. It is not intended that objects of this class need ever be instantiated by ObjectARX applications, or that this class ever be derived from.  
 AcDbVertexRef  Reference to a vertex, i.e. a subentity whose geometry is a point. It may either be whole AcDbPoint entity, kVertexSubentType subentity of an entity, or a constant AcGePoint3d. Notice that this reference is generally non-persistent, because it uses transient AcDbSubentId.   
 AcDbViewBorder  The AcDbViewBorder class represents the border entity used by model documentation views in association with an AcDbViewport.  
 AcDbViewport  The AcDbViewport class represents the VIEWPORT entity within AutoCAD.  
 AcDbViewportTable  This class is the symbol table for AcDbViewportTableRecords, which represent viewport configurations within AutoCAD when the system variable TILEMODE == 1. Within AutoCAD and DXF files this symbol table is known as the "VPORT" table.  
 AcDbViewportTableIterator  This class is for iterating over the records in an AcDbViewportTable.
The only substantial difference from the base class AcDbAbstractViewTableIterator is the use of AcDbViewportTableRecord-specific signatures for the member functions getRecord() and seek().  
 AcDbViewportTableRecord  Objects of this class represent viewport arrangements when TILEMODE == 1 within AutoCAD (the ViewportTable and its records are not used when TILEMODE == 0). The AcDbViewportTable (referred to in AutoCAD and DXF documentation as the VPORT table) is the container or owner of objects of this class.
The AcDbViewportTable is unique in that it can contain multiple AcDbViewportTableRecords with the same name. This is necessary because when a multiple viewport configuration is saved, all viewports in the configuration must share the name, but each viewport has its own AcDbViewportTableRecord created to save the data for that viewport.... more  
 AcDbViewRepBlockReference  Thje AcDbViewRepBlockReference class is derived from AcDbBlockReference and used by the model documentation feature to reference a block table record that is associated to a specific AcDbViewport.  
 AcDbViewSymbol  Abstract base class used by AcDbSectionSymbol and AcDbDetailSymbol.  
 AcDbViewTable  This class is the symbol table for AcDbViewTableRecords, which represent stored views within the drawing database. Within AutoCAD and DXF files this table is known as the "VIEW" table.  
 AcDbViewTableIterator  This class is for iterating over the records in an AcDbViewTable.
The only substantial difference from the base class AcDbAbstractViewTableIterator is the use of AcDbViewTableRecord-specific signatures for the member functions getRecord() and seek().  
 AcDbViewTableRecord  Objects of this class represent stored views within AutoCAD. The AcDbViewTable object (referred to in the AutoCAD and DXF documentation as the VIEW table) within each database is the container or owner of objects of this class.
Most of the data and functionality of this class is inherited from its AcDbAbstractViewTableRecord parent class.
Within AutoCAD, objects of this class are created whenever the VIEW command's "save" option is used to save a view with a name that does not already exist.  
 AcDbVisibilityOverrule  AcDbVisibilityOverrule overrules a subset of visibility-related operations that AcDbEntity class specifies. It is intended as a base class for clients who want to alter some or all behavior of a given AcDbEntity-derived class. At the base level, each default implementation simply calls the corresponding method in the target class.  
 AcDbVisualStyle  This class describes the visual style database object, which contains a collection of properties that can be applied to faces, edges and the display.  
 AcDbWblockCloneFiler  This class maintains a queue of typed (that is, AcDbHardOwnershipId, AcDbSoftOwnershipId, etc.) object IDs. All read and write requests are forwarded to an embedded "real" filer, except for object IDs that are removed from or added to the typed object ID queue. The class methods provide protocol for reading or writing objects in the queue that are "hard" referenced by other objects being wblockCloned (the wblockClone mechanism follows hard references).  
 AcDbWipeout  Objects of the AcDbWipeout class are polygonally clipped raster entities that are used as masks to cover parts of other entities in the drawing. The most common use is as a background mask for text entities.
To use this class, acismobj20.dbx must be loaded so that the AcDbWipeout class implementation code is available.  
 AcDbXline  The AcDbXline class represents the XLINE entity type within AutoCAD. An xline entity is a line that extends to infinity in both directions.  
 AcDbXObject  The original purpose of this class was to serve as a lightweight fragment of persistence, typically to be added to AcDbObjects. But this was never implemented, so now it merely serves as a parent class to AcCmComplexColor, which itself is a class that is not used by AutoCAD or the ObjectARX libraries.  
 AcDbXrecord  The AcDbXrecord class is a data storage class that was implemented primarily to allow ADSRX and AutoLISP programs a means to store data in pieces larger than the xdata's 16KB-per-object limit. Each AcDbXrecord object is capable of storing up to 2GB. By establishing an object's extension dictionary as the xrecord's owner, it's possible to associate large amounts of data with that object.
The input or output mechanism for the data is via a linked list of resbuf structures very similar to the way xdata is accessed on objects. Unlike xdata, however, xrecords work with the standard AutoCAD group codes, which... more  
 AcDbXrecordIterator  Objects of the AcDbXrecordIterator class provide an easy mechanism to iterate through the resbuf structures that make up the data lists stored within AcDbXrecord objects.  
 AcDbXrefGraph  AcDbXrefGraph is a derived class for representing xrefs, using the AcDbXrefGraphNode class to represent one xref database at each node.
An AcDbXrefGraph is a representation of the relationship between a host drawing, its xref'ed drawings, and any nested xref drawings. Each database or xref block table record is represented by an AcDbXrefGraphNode in the graph. The host drawing is always the rootNode. Each reference (between databases) is represented by an edge in the graph, and can be queried by calling AcDbGraphNode::in(idx) for what is referencing this node, and AcDbGraphNode::out(idx) for what this node references.
While there can be more... more  
 AcDbXrefGraphNode  AcDbXrefGraph is a derived class for representing an xref. Each AcDbXrefGraphNode represents one xref database.
An AcDbXrefGraph is a representation of the relationship between a host drawing, its xref'd drawings, and any nested xref drawings. Each database or xref BlockTableRecord is represented by an AcDbXrefGraphNode in the graph. The host drawing is always the rootNode. Each reference (between databases) is represented by an edge in the graph, and can be queried by calling AcDbGraphNode::in(idx) for what is referencing this node, and AcDbGraphNode::out(idx) for what this node references.
While there can be more than one AcDbBlockReference referencing an xref AcDbBlockTableRecord... more  
 AcDbXrefObjectId  The purpose of this class is to provide a convenient way to refer to a specific AcDbObject
within an xref'd database. AcDbXrefObjectId is persistent across drawing sessions.  
 AcDistanceConstraint  This class represents a distance constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two constrained geometries (normally two points).   
 AcEqualCurvatureConstraint  This class represents a EqualCurvature constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between a bounded spline and a bounded curve.   
 AcEqualDistanceConstraint  This class represents a EqualDistance constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two pairs of points.   
 AcEqualHelpParameterConstraint  This class represents a EqualHelpParameter constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two AcHelpParameter objects which belong to the same spline or ellipse.   
 AcEqualLengthConstraint  This class represents a EqualLength constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two constrained bounded lines (not rays).   
 AcEqualRadiusConstraint  This class represents a EqualRadius constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two constrained circles (arcs).   
 AcExplicitConstraint  This class represents a dimensional constraint node in the owning AcDbAssoc2dConstraintGroup. It is the base class for all the kind of dimensional constraints.   
 AcFixedConstraint  This class represents a Fixed constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied on any constrained geometry.   
 AcG2SmoothConstraint  This class represents a G2Smooth composite constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between a constrained bounded spline and other constrained bounded curve. It is a combination of AcTangentConstraint and AcEqualCurvatureConstraint.   
 AcGeomConstraint  This class represents a constraint node in the owning AcDbAssoc2dConstraintGroup. It is the base class for all the kind of constraints, including geometrical and dimensional constraints.   
 AcGraph  The AcGraph class represents a graph which manages a set of AcGraphNode with a unique id.  
 AcGraphNode  The AcGraph class represents a graph that manages a set of AcGraphNode with a unique ID.  
 AcHelpParameter  This class represents a help parameter node in the owning AcDbAssoc2dConstraintGroup. Help parameters are only applicable for splines and ellipses and are required for some kind of constraints. </para> <para xmlns="http://buildtree.tv/SpecSchema.xsd"> A AcHelpParameter object is always connected with a AcGeomConstraint object and a AcConstrainedGeometry object. </para>  
 AcHorizontalConstraint  This class represents a Horizontal constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied on one constrained line.   
 AcMidPointConstraint  This class represents a MidPoint constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between a point and a bounded line (not ray) or arc.   
 AcNormalConstraint  This class represents a Normal constraint node in the owning AcDbAssoc2dConstraintGroup. Currently tt can only be applied between a line and circle(or arc).   
 AcParallelConstraint  This class represents a Parallel constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two constrained lines.   
 AcPerpendicularConstraint  This class represents a Perpendicular constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two lines.   
 AcPointCoincidenceConstraint  This class represents a PointCoincidence (coincident) constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two constrained point.   
 AcPointCurveConstraint  This class represents a PointCurve (coincident) constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between a constrained point and a constrained curve.   
 AcRadiusDiameterConstraint  This class represents a radius or diameter constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied on one circle(arc) or ellipse(bounded ellipse).   
 AcSymmetricConstraint  This class represents a Symmetric constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two same type of constrained geometries (except spline).   
 AcTangentConstraint  This class represents a Tangent constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied between two constrained curve.   
 AcVerticalConstraint  This class represents a Vertical constraint node in the owning AcDbAssoc2dConstraintGroup. It can be applied on one constrained line.   
 WritableAcDbObject  This class organizes matching calls to accessAcDbObjectForWrite and revertAcDbObjectFromWrite to ease their use. All the caller supplies is a reference to an AcDbObject pointer. If the pointer is non-null after the constructor, then it can be used for writing, and the object open state is reverted is closed when the instance destructor is invoked. enableNow can be used to defer write enabled status.
While existing internal callers are free to continue using this class, new callers are strongly encouraged to use AcDbSmartObjectPointer only.  

Enumerations
Enumeration  Description  
 AcDbAssocConstraintType  This enum describes dimensional constraint types.  
 AcDbAssocCreateImpObject  Internal use only.  
 AcDbAssocDraggingState  Indicates during the evaluation whether the evaluation is taking place from inside a dragging loop as well as the stage of the dragging.  
 AcDbAssocEvaluationMode  This enum specifies the evaluation mode to use in evaluateOverride.  
 AcDbAssocEvaluationPriority  A negative priority mean that an AcDbAssocAction cannot be evaluated at this time, positive priority means that the action can be evaluated now. A higher positive priority should be evaluated before actions with lower positive priority. The negative priority means how much the action cannot be evaluated. The smaller the number, the less the action can be evaluated.  
 AcDbAssocStatus  Indicates the status of AcDbAssocActions and AcDbAssocDependencies.
In the case of an action it relates to the action itself. In case of a dependency it relates to the object the dependency depends on.  
 AcDbAssocTransformationType  
Information about what type of transformation (which AutoCAD command) has been performed with the entities before the actions that have dependencies on these entities are evaluated.  

Methods
Method  Description  
 AcDbAssocDependencyNotificationDisabler  Default constructor. It disables the notification to dependencies.  

Namespaces
Namespace  Description  
AcDbDictUtil  The following are some dictionary utility functions similar to those defined for symbol tables in dbsymutl.h.  
AcDbGripOperations  This namespace provides scoping for enums and constants that are used by the AcDbGripData class.  
AcDbIndexFilterManager  AcDbIndexFilterManager is a namespace that provides a collection of functions for index and filter access and maintenance functionality.  
AcDbSymbolUtilities  Namespace AcDbSymbolUtilities contains various utilities for working with symbol names, symbol records, and symbol tables. The main access to the utility functions is through acdbSymUtil(), which is dereferenced to call the member functions of class AcDbSymbolUtilities::Services.

The namespace alias AcDbSymUtil is available for use as a shorter abbreviation for the full namespace AcDbSymbolUtilities.

This namespace may be used in ObjectARX and ObjectDBX modules.  

Structures
Structure  Description  
 AcDbAssocTransInfo  This struct contains information about the transformation applied on associative geometries.  

Templates
Template  Description  
 AcDbAssocObjectPointer  When an action is being evaluated and needs to use or modify an existing object it has a dependency on, the client code is given an option to control which object the action is actually going to use or modify. The client code can provide a substitute object instead of the original object by implementing the AcDbAssocEvaluationCallback::beginActionEvaluationUsingObject() callback method. This way the action evaluation can be redirected to use or modify another object (see the comments at the AcDbAssocEvaluationCallback class).

The AcDbAssocObjectPointer template class, modeled after the AcDbObjectPointer template class, simplifies the client action evaluation code that needs to respect... more  
 AcDbObjectPointer  This class allows you to access any AcDbObject-based object given its object ID.

The following pre-defined typedefs are available: 
AcDbDictionaryPointer 
AcDbEntityPointer 

These classes are designed to be type-safe replacements for explicitly using acdbOpenObject(), acdbOpenAcDbObject(), and acdbOpenAcDbEntity(). Using these classes incurs opening and closing objects, which, under certain circumstances, may be better managed using transactions.  
 AcDbObjectPointerBase  AcDbObjectPointerBase is the base class for a group of template classes that simplify memory management of pointers to AcDbObjects and opening and closing AcDbObjects.
These classes provide automatic opening and closing of AcDbObjects upon construction and destruction. The constructor provides the necessary arguments to open the object, and the destructor closes the object. During the lifetime of the object, clients use operator->() to manipulate the opened object.
AcDbObjectPointerBase provides the basic services of opening and closing objects given object IDs and defines the basic contracts for using the smart pointers derived from it. Derived classes provide the... more  
 AcDbSmartObjectPointer  This class is protocol-compatible with AcDbObjectPointer and has the added capability to avoid open conflicts to access an object when given an object id, in addition to the longstanding capability to always "close" an object or at least revert it to the open state it was in prior to being assigned to the pointer.
The intent is to replace the implementation of AcDbObjectPointer with AcDbSmartObjectPointer logic in a future release. AcDbSmartObjectPointer works by NOT opening an object at all if it's open state is already what was requested, or even closing an object multiple times before opening in the desired... more  
 AcDbSymbolTablePointer  This class allows you to access the symbol tables associated with every AcDbDatabase. You can specify an object ID or a particular database.
The following pre-defined typedefs are available for individual types of symbol tables:
AcDbBlockTablePointer
AcDbDimStyleTablePointer
AcDbLayerTablePointer
AcDbLinetypeTablePointer
AcDbRegAppTablePointer
AcDbTextStyleTablePointer
AcDbUCSTablePointer
AcDbViewTablePointer
AcDbViewportTablePointer

These classes are designed to be type-safe replacements for explicitly using acdbOpenObject(), acdbOpenAcDbObject(), and acdbOpenAcDbEntity(). Using these classes incurs opening and closing objects, which, under certain circumstances, may be better managed using transactions.  
 AcDbSymbolTableRecordPointer  This class allows you to access symbol table records by object ID or by name.
The following pre-defined typedefs are available for individual types of symbol table records:
AcDbBlockTableRecordPointer
AcDbDimStyleTableRecordPointer
AcDbLayerTableRecordPointer
AcDbLinetypeTableRecordPointer
AcDbRegAppTableRecordPointer
AcDbTextStyleTableRecordPointer
AcDbUCSTableRecordPointer
AcDbViewTableRecordPointer
AcDbViewportTableRecordPointer

These classes are designed to be type-safe replacements for explicitly using acdbOpenObject(), acdbOpenAcDbObject(), and acdbOpenAcDbEntity(). Using these classes incurs opening and closing objects, which, under certain circumstances, may be better managed using transactions.  
 ReadableAcDbObject  This class manages calls to accessAcDbObjectForRead and then reverts the open state change, if any, from the destructor or a call to the revertRead() member. While existing internal callers are free to continue using this class, new callers are strongly encouraged to use AcDbSmartObjectPointer only.  

Types
Type  Description  
AcDbDimDataPtrArray  An AcDbDimDataPtrArray is a dynamic array of pointers to AcDbDimData objects. It is a template class based on the AcArray template class. See the description of the AcArray class for information on operators and methods.
  
AcDbIntArray  AcDbIntArray is a template class that uses the AcArray class template. Please see AcArray for a complete description of the AcArray operators and methods.  
AcDbObjectIdArray  AcDbObjectIdArray is a template class that uses the AcArray class template. Please see AcArray for a complete description of the AcArray operators and methods.  
AcDbVoidPtrArray  AcDbVoidPtrArray is a template class that uses the AcArray class template. Please see AcArray for a complete description of the AcArray operators and methods.  

AcDbClassIterator Class
AcDbClassIterator Class  Description  
 ~AcDbClassIterator  This is ~AcDbClassIterator, a member of class AcDbClassIterator.  
 AcDbClassIterator  This is AcDbClassIterator, a member of class AcDbClassIterator.  
AcDbClassIterator Methods    

ACGS

 AcGs2DViewLimitManager  Use acgsCreate2DViewLimitManager() and acgsDestroy2DViewLimitManager() to create and destroy instances of this pure virtual abstract class.
  
 AcGsConfig  The AcGsConfig object is used to configure the GS. This class is a singleton; there is exactly one instance of this class at all times. Various included functions perform the following operations:

The tessellation functions set options that determine the smoothness of objects when drawn. Tessellation lines are a series of short lines (or triangles in some cases) that the Graphics System uses to draw objects. Objects appear smoother when there are more tessellation lines. 
The render option functions configure the Graphics System to make settings available for enhancing the display of lights, materials, textures, and transparency. 
The adaptive degradation... more 
 
 AcGsConfigReactor  New reactor for use with Gs Configuration changes  
 AcGsDCPoint  The AcGsDCPoint class represents a point in device coordinates. This class is used in a variety of graphics system (GS) APIs.  
 AcGsDCRect  The AcGsDCRect class represents a rectangle in device coordinates. This class is used in a variety of graphics system (GS) APIs.  
 AcGsDevice  AcGsDevice is an abstraction of a drawing surface and encapsulates the underlying device to which graphics are rendered. At present, an AcGsDevice corresponds to a GUI display window. Eventually, other classes of devices will be supported (such as hardcopy, file [.bmp, .wmf, .dwf], and clipboard).
An AcGsDevice owns one or more AcGsViews and must manage damaged areas and delegate updates to these views. In addition, it responds to operating system notifications such as a change in window size forwarded from the underlying device by the graphics system (GS) client.  
 AcGsGraphicsKernel  This class is used in graphics system (GS) initialization and shutdown. The GS meta-API objects hide behind a pure virtual interface, and they are constructed via the AcGsGraphicsKernel.  
 AcGsKernelDescriptor  The kernel descriptor contains a set of capabilities that describe a graphics kernel. This class can be used to request a particular kernel, or to report the capabilities of a kernel.  
 AcGsManager  The AcGsManager interface provides services that helps to hide the details of connecting AutoCAD and ARX applications to the graphics system (GS).
Each AutoCAD AcDisplay object has an AcGsManager; thus, AcGsManager objects have a one-to-one relationship with MDI client windows and with AcDbDatabase objects. The function acgsGetGSManager() provides access to AcGsManager objects.  
 AcGsModel  The AcGsModel class is an abstraction of a database. This database may be an AcDbDatabase, it may be a container for transient graphics (such as the Icon and Grid), or it may represent another persistent database format such as DWF. Most significantly, the AcGsModel provides a mechanism for the database to inform the graphics system (GS) of changes (additions, deletions, and modifications).
Each AcGsModel has a RenderType that provides a hint to the GS about how this model's geometry should be rendered.  
 AcGsModelReactor  The AcGsModelReactor class is used to inform events of AcGsModel.  
 AcGsNode  This class represents a node in the GS.
  
 AcGsReactor  The AcGsReactor object is used to inform interested parties of important Graphics System (GS) events. To make use of this functionality, derive a reactor class from AcGsReactor and provide implementations for those notifications they wish to receive. Then, pass an instance of this class to AcGsClassFactory::addReactor(). Be sure to remove this reactor instance via AcGsClassFactory::removeReactor() when notifications are no longer required.  
 AcGsRenderInterrupter    
 AcGsRenderProgressMonitor    
 AcGsScreenShot  Objects of the AcGsScreenShot class are used by the acgsGetScreenShot() function as a means to return viewport image information (such as width, height, color depth and palette, and scanlines) to calling applications.  
 AcGsView  An AcGsView object corresponds to a portion of a GUI window, a set of camera parameters that define a view, and a collection of objects to be viewed. More specifically, an AcGsView can be used to represent an AutoCAD viewport.
An AcGsView is added to an AcGsDevice. A view may belong to only a single device at a time. Views need to be told what they are viewing. The client adds and removes (AcGiDrawable, AcGsModel) pairs corresponding to the viewed piece of the specified model. Each AcGsView has a RenderMode that corresponds to the algorithm employed when drawing geometry in... more  
