/**
 *  @file   PandoraSDK/include/Pandora/PandoraObjectFactories.h
 * 
 *  @brief  Header file for the pandora object factories classes.
 * 
 *  $Log: $
 */
#ifndef PANDORA_OBJECT_FACTORIES_H
#define PANDORA_OBJECT_FACTORIES_H 1

#include "Pandora/ObjectFactory.h"
#include "Pandora/StatusCodes.h"

namespace pandora
{

/**
 *  @brief  PandoraObjectFactory class
 *
 *  The SDK default factory. All Read/Write methods are no-ops: the SDK
 *  readers/writers handle all standard fields directly; this factory has
 *  no experiment-specific extra fields to contribute.
 *
 *  Both generation-1 and generation-2 overrides are provided so the
 *  default factory explicitly satisfies the full base-class interface
 *  rather than inheriting the generation-2 NOT_IMPLEMENTED defaults.
 */
template <typename PARAMETERS, typename OBJECT>
class PandoraObjectFactory : public ObjectFactory<PARAMETERS, OBJECT>
{
public:
    typedef PARAMETERS Parameters;
    typedef OBJECT     Object;

    Parameters *NewParameters() const;

    // Generation-2 no-ops
    StatusCode Read(Parameters &parameters, const FieldMap &fields) const;
    StatusCode Write(const Object *const pObject, FieldMap &fields) const;

    // Generation-1 no-ops (satisfy pure-virtual base; deprecated)
    StatusCode Read(Parameters &parameters, FileReader &fileReader) const;
    StatusCode Write(const Object *const pObject, FileWriter &fileWriter) const;

private:
    StatusCode Create(const Parameters &parameters, const Object *&pObject) const;
};

} // namespace pandora

#endif // #ifndef PANDORA_OBJECT_FACTORIES_H
