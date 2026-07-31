/**
 *  @file   PandoraSDK/include/Persistency/BinaryFileWriter.h
 *
 *  @brief  Header file for the binary file writer class.
 *
 *  $Log: $
 */
#ifndef PANDORA_BINARY_FILE_WRITER_H
#define PANDORA_BINARY_FILE_WRITER_H 1

#include "Pandora/Pandora.h"

#include "Objects/CartesianVector.h"
#include "Objects/TrackState.h"

#include "Persistency/FileWriter.h"
#include "Persistency/Persistency.h"   // FieldMap

#include <fstream>

namespace pandora
{

/**
 *  @brief  BinaryFileWriter
 *
 *  Writes Pandora objects to a compact binary file using tagged-field component
 *  records. Each component is framed as:
 *
 *      [ComponentId  : uint32]
 *      [SchemaVersion: uint32]
 *      [NumFields    : uint32]
 *      repeated NumFields times:
 *          [TagLength : uint16]
 *          [Tag       : char * TagLength]   (not null-terminated)
 *          [DataLength: uint32]
 *          [Data      : byte * DataLength]
 *      [END_MARKER   : uint32 = 0xDEADBEEF]
 *
 *  Container framing (EVENT / GEOMETRY / HEADER) is identical to the original
 *  format: a "pandora" hash, ContainerId, and a back-patched byte-size field.
 *  This means container-level navigation (GoToEvent, GoToGeometry) is unaffected.
 *
 *  Global header
 *  -------------
 *  WriteGlobalHeader (overridden here) writes FileMetadata and a SchemaRegistry
 *  before the component end-marker, providing file-level self-description.
 *
 *  Adding a new field
 *  ------------------
 *  Add one WriteField call in the relevant Write* method and increment that
 *  component's schema version in SCHEMA_VERSIONS. No other changes are needed —
 *  readers that do not know the new field skip it via DataLength.
 */
class BinaryFileWriter : public FileWriter
{
public:
    /**
     *  @brief  Constructor
     *
     *  @param  pandora       the pandora instance to be used alongside the file writer
     *  @param  fileName      the name of the output file
     *  @param  fileMode      APPEND (default) or OVERWRITE
     *  @param  majorVersion  legacy file major version (carried forward, not used by new reader)
     *  @param  minorVersion  legacy file minor version (carried forward, not used by new reader)
     */
    BinaryFileWriter(const pandora::Pandora &pandora, const std::string &fileName, const FileMode fileMode = APPEND,
        const unsigned int majorVersion = 1, const unsigned int minorVersion = 0);

    /**
     *  @brief  Destructor
     */
    ~BinaryFileWriter();

    /**
     *  @brief  Write the global header (overrides FileWriter::WriteGlobalHeader).
     *          Serialises FileMetadata and the SchemaRegistry in addition to the
     *          VERSION_COMPONENT written by the base implementation.
     */
    StatusCode WriteGlobalHeader();

    /**
     *  @brief  Write a primitive value directly to the stream (low-level helper).
     *          Used internally and by ObjectFactory::Write implementations.
     */
    template <typename T>
    StatusCode WriteVariable(const T &t);

private:
    // -----------------------------------------------------------------------
    // Container framing (unchanged from original)
    // -----------------------------------------------------------------------
    StatusCode WriteHeader(const ContainerId containerId);
    StatusCode WriteFooter();

    // -----------------------------------------------------------------------
    // Global-header components
    // -----------------------------------------------------------------------
    StatusCode WriteVersion();
    StatusCode WriteMetadata();
    StatusCode WriteSchemaRegistry();

    // -----------------------------------------------------------------------
    // Component writers — all delegate to WriteComponent(id, version, map)
    // -----------------------------------------------------------------------
    StatusCode WriteSubDetector(const SubDetector *const pSubDetector);
    StatusCode WriteLArTPC(const LArTPC *const pLArTPC);
    StatusCode WriteDetectorGap(const DetectorGap *const pDetectorGap);
    StatusCode WriteCaloHit(const CaloHit *const pCaloHit);
    StatusCode WriteTrack(const Track *const pTrack);
    StatusCode WriteMCParticle(const MCParticle *const pMCParticle);
    StatusCode WriteRelationship(const RelationshipId relationshipId, const void *address1, const void *address2, const float weight);
    StatusCode WriteEventInformation();

    /**
     *  @brief  Serialise a fully-populated FieldMap as a tagged-field component record.
     *
     *  @param  componentId    the component type identifier
     *  @param  schemaVersion  schema version for this component type
     *  @param  fields         the populated field map
     */
    StatusCode WriteComponent(const ComponentId componentId, const unsigned int schemaVersion, const FieldMap &fields);

    // -----------------------------------------------------------------------
    // Schema version table
    // -----------------------------------------------------------------------
    /**
     *  @brief  Return the current schema version for a given component type.
     *          Increment a value here (and register a reader migration) whenever
     *          a field is removed or its meaning changes. Adding new optional
     *          fields does NOT require a version bump.
     */
    static unsigned int GetSchemaVersion(const ComponentId componentId);

    // -----------------------------------------------------------------------
    // End-of-component sentinel value
    // -----------------------------------------------------------------------
    static constexpr uint32_t COMPONENT_END_MARKER = 0xDEADBEEFu;

    // -----------------------------------------------------------------------
    // Data members
    // -----------------------------------------------------------------------
    std::ofstream::pos_type m_containerPosition; ///< Start of current container (for back-patching size)
    std::ofstream           m_fileStream;         ///< Output stream
};

//------------------------------------------------------------------------------------------------------------------------------------------
// WriteVariable template implementations (low-level stream primitives)
// These are unchanged in behaviour from the original; they exist so that
// ObjectFactory::Write implementations continue to compile without modification.
//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
inline StatusCode BinaryFileWriter::WriteVariable(const T &t)
{
    m_fileStream.write(reinterpret_cast<const char *>(&t), sizeof(T));

    if (!m_fileStream.good())
        return STATUS_CODE_FAILURE;

    return STATUS_CODE_SUCCESS;
}

template <>
inline StatusCode BinaryFileWriter::WriteVariable(const std::string &t)
{
    const uint32_t stringSize(static_cast<uint32_t>(t.size()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(stringSize));
    m_fileStream.write(t.c_str(), stringSize);

    if (!m_fileStream.good())
        return STATUS_CODE_FAILURE;

    return STATUS_CODE_SUCCESS;
}

template <>
inline StatusCode BinaryFileWriter::WriteVariable(const CartesianVector &t)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(t.GetX()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(t.GetY()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(t.GetZ()));
    return STATUS_CODE_SUCCESS;
}

template <>
inline StatusCode BinaryFileWriter::WriteVariable(const TrackState &t)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(t.GetPosition()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(t.GetMomentum()));
    return STATUS_CODE_SUCCESS;
}

} // namespace pandora

#endif // #ifndef PANDORA_BINARY_FILE_WRITER_H
