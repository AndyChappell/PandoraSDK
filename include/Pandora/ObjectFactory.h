/**
 *  @file   PandoraSDK/include/Pandora/ObjectFactory.h
 * 
 *  @brief  Header file for the object factory class.
 * 
 *  $Log: $
 */
#ifndef PANDORA_OBJECT_FACTORY_H
#define PANDORA_OBJECT_FACTORY_H 1

#include "Pandora/StatusCodes.h"

#include "Persistency/FieldMap.h"

namespace pandora
{

class FileReader;
class FileWriter;

//------------------------------------------------------------------------------------------------------------------------------------------

/**
 *  @brief  ObjectFactory class responsible for extended pandora object creation
 *
 *  Read / Write interface
 *  ----------------------
 *  Two generations of Read/Write exist simultaneously to allow incremental
 *  migration of experiment-specific factory subclasses without a flag day.
 *
 *  Generation 1 (legacy, retained for backward compatibility):
 *    virtual StatusCode Read (Parameters &, FileReader &) const
 *    virtual StatusCode Write(const Object *, FileWriter &) const
 *
 *  Generation 2 (current, format-agnostic):
 *    virtual StatusCode Read (Parameters &, const FieldMap &) const
 *    virtual StatusCode Write(const Object *, FieldMap &) const
 *
 *  The SDK readers/writers call the generation-2 overloads exclusively.
 *  A factory that has not yet provided generation-2 overrides will have
 *  STATUS_CODE_NOT_IMPLEMENTED returned from the base defaults, which the
 *  SDK treats as "no extra fields" — the same observable result as the
 *  generation-1 no-ops.
 *
 *  Migration path for user factories
 *  ----------------------------------
 *  1. Add Read(Parameters &, const FieldMap &) const override.
 *  2. Add Write(const Object *, FieldMap &) const override.
 *  3. Remove the old generation-1 overrides when convenient.
 *  Until step 3 the factory compiles and links correctly; the generation-1
 *  methods are simply no longer called by the SDK.
 */
template <typename PARAMETERS, typename OBJECT>
class ObjectFactory
{
public:
    typedef PARAMETERS Parameters;
    typedef OBJECT     Object;

    /**
     *  @brief  Default constructor
     */
    ObjectFactory();

    /**
     *  @brief  Destructor
     */
    virtual ~ObjectFactory();

    /**
     *  @brief  Create new parameters instance on the heap (caller takes ownership)
     */
    virtual Parameters *NewParameters() const = 0;

    // -----------------------------------------------------------------------
    // Generation-2 interface (format-agnostic, FieldMap-based)
    // -----------------------------------------------------------------------

    /**
     *  @brief  Read any additional (derived class only) object parameters from
     *          the supplied FieldMap. Called by SDK readers; use GetOrDefault so
     *          that absent fields (old files) are handled gracefully.
     *
     *  @param  parameters  the parameters to populate
     *  @param  fields      the FieldMap built by the format reader
     *
     *  @return STATUS_CODE_SUCCESS on success,
     *          STATUS_CODE_NOT_IMPLEMENTED if not overridden (treated as no-op)
     */
    virtual StatusCode Read(Parameters &parameters, const FieldMap &fields) const;

    /**
     *  @brief  Persist any additional (derived class only) object parameters
     *          into the supplied FieldMap. Called by SDK writers; use
     *          fields.Set(tag, value) for each custom field.
     *
     *  @param  pObject  the object to persist
     *  @param  fields   the FieldMap to populate
     *
     *  @return STATUS_CODE_SUCCESS on success,
     *          STATUS_CODE_NOT_IMPLEMENTED if not overridden (treated as no-op)
     */
    virtual StatusCode Write(const Object *const pObject, FieldMap &fields) const;

    // -----------------------------------------------------------------------
    // Generation-1 interface (legacy, deprecated but retained)
    // -----------------------------------------------------------------------

    /**
     *  @brief  Read any additional (derived class only) object parameters from
     *          file using the specified file reader. Deprecated: override the
     *          FieldMap-based Read instead.
     *
     *  @param  parameters  the parameters to pass in constructor
     *  @param  fileReader  the file reader
     */
    virtual StatusCode Read(Parameters &parameters, FileReader &fileReader) const = 0;

    /**
     *  @brief  Persist any additional (derived class only) object parameters
     *          using the specified file writer. Deprecated: override the
     *          FieldMap-based Write instead.
     *
     *  @param  pObject     the address of the object to persist
     *  @param  fileWriter  the file writer
     */
    virtual StatusCode Write(const Object *const pObject, FileWriter &fileWriter) const = 0;

protected:
    /**
     *  @brief  Create an object with the given parameters
     *
     *  @param  parameters  the parameters to pass in constructor
     *  @param  pObject     receives the address of the object created
     */
    virtual StatusCode Create(const Parameters &parameters, const Object *&pObject) const = 0;

    friend class CaloHitManager;
    friend class TrackManager;
    friend class MCManager;
    friend class ClusterManager;
    friend class VertexManager;
    friend class ParticleFlowObjectManager;
    friend class GeometryManager;
};

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename PARAMETERS, typename OBJECT>
inline ObjectFactory<PARAMETERS, OBJECT>::ObjectFactory()
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename PARAMETERS, typename OBJECT>
inline ObjectFactory<PARAMETERS, OBJECT>::~ObjectFactory()
{
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Generation-2 defaults — return NOT_IMPLEMENTED so SDK readers/writers
// treat them as "no extra fields" without needing a special-case check.
//------------------------------------------------------------------------------------------------------------------------------------------

template <typename PARAMETERS, typename OBJECT>
inline StatusCode ObjectFactory<PARAMETERS, OBJECT>::Read(Parameters &/*parameters*/, const FieldMap &/*fields*/) const
{
    return STATUS_CODE_NOT_IMPLEMENTED;
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename PARAMETERS, typename OBJECT>
inline StatusCode ObjectFactory<PARAMETERS, OBJECT>::Write(const OBJECT *const /*pObject*/, FieldMap &/*fields*/) const
{
    return STATUS_CODE_NOT_IMPLEMENTED;
}

} // namespace pandora

#endif // #ifndef PANDORA_OBJECT_FACTORY_H
