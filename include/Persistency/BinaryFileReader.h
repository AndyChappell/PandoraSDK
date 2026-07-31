/**
 *  @file   PandoraSDK/include/Persistency/BinaryFileReader.h
 *
 *  @brief  Header file for the binary file reader class.
 *
 *  $Log: $
 */
#ifndef PANDORA_BINARY_FILE_READER_H
#define PANDORA_BINARY_FILE_READER_H 1

#include "Pandora/Pandora.h"

#include "Objects/CartesianVector.h"
#include "Objects/TrackState.h"

#include "Persistency/FileReader.h"
#include "Persistency/Persistency.h"   // FieldMap

#include <fstream>
#include <functional>
#include <unordered_map>

namespace pandora
{

/**
 *  @brief  BinaryFileReader
 *
 *  Reads Pandora objects from a binary file written by BinaryFileWriter. The
 *  file format uses tagged-field component records (see BinaryFileWriter.h for
 *  the full wire layout), so this reader is forward- and backward-compatible at
 *  the per-field level independently for each component type.
 *
 *  Dispatch
 *  --------
 *  ReadNextGlobalHeaderComponent / ReadNextGeometryComponent / ReadNextEventComponent
 *  each call the unified ReadNextComponent(), which:
 *    1. Reads the component header (ComponentId, schemaVersion, numFields).
 *    2. Reads all tagged fields into a FieldMap.
 *    3. Verifies the COMPONENT_END_MARKER.
 *    4. Applies any registered migration chain if schemaVersion < current.
 *    5. Dispatches to the appropriate typed deserialiser via m_componentHandlers.
 *
 *  Unknown component IDs are skipped cleanly — the FieldMap is built and then
 *  discarded. This means a reader compiled against an old SDK can process files
 *  written by a newer one without crashing, though it will skip unrecognised
 *  component types.
 *
 *  Migration registration
 *  ----------------------
 *  Call RegisterMigration() before reading to install a schema upgrade function
 *  for a given component type and version transition. Migrations are chained
 *  automatically: a file at schema version 1 opened by a reader that knows
 *  versions up to 3 will have the 1→2 and 2→3 functions applied in sequence.
 *
 *  Example:
 *  @code
 *      reader.RegisterMigration(CALO_HIT_COMPONENT, 1, 2,
 *          [](FieldMap &fields) {
 *              float v;
 *              if (STATUS_CODE_SUCCESS == fields.Get("mipEnergy", v))
 *              {
 *                  fields.Set("mipEquivalentEnergy", v);
 *                  fields.Remove("mipEnergy");
 *              }
 *          });
 *  @endcode
 *
 *  ReadVariable
 *  ------------
 *  The low-level ReadVariable<T> template is kept public so that
 *  ObjectFactory::Read implementations continue to compile without changes
 *  during the transition period (they currently call reader.ReadVariable).
 *  Once all factories are migrated to the FieldMap-based interface, this
 *  method can be made private.
 */
class BinaryFileReader : public FileReader
{
public:
    /**
     *  @brief  Constructor
     *
     *  @param  pandora   the pandora instance to be used alongside the file reader
     *  @param  fileName  the name of the file containing the pandora objects
     */
    BinaryFileReader(const pandora::Pandora &pandora, const std::string &fileName);

    /**
     *  @brief  Destructor
     */
    ~BinaryFileReader();

    /**
     *  @brief  A migration function transforms a FieldMap from one schema version
     *          to the next for a given component type.
     */
    typedef std::function<void(FieldMap &)> MigrationFn;

    /**
     *  @brief  Register a schema migration for a component type.
     *
     *  @param  componentId   the component type this migration applies to
     *  @param  fromVersion   the schema version present in the file
     *  @param  toVersion     the schema version after this migration (must be fromVersion + 1)
     *  @param  fn            the migration function
     */
    void RegisterMigration(const ComponentId componentId, const unsigned int fromVersion,
        const unsigned int toVersion, MigrationFn fn);

    /**
     *  @brief  Read a primitive value directly from the stream (low-level helper).
     *          Public for backward compatibility with ObjectFactory::Read
     *          implementations; prefer FieldMap-based access in new code.
     */
    template <typename T>
    StatusCode ReadVariable(T &t);

private:
    // -----------------------------------------------------------------------
    // FileReader pure-virtual interface
    // -----------------------------------------------------------------------
    StatusCode ReadHeader();
    StatusCode GoToNextContainer();
    ContainerId GetNextContainerId();
    StatusCode GoToGeometry(const unsigned int geometryNumber);
    StatusCode GoToEvent(const unsigned int eventNumber);
    StatusCode ReadNextGlobalHeaderComponent();
    StatusCode ReadNextGeometryComponent();
    StatusCode ReadNextEventComponent();

    // -----------------------------------------------------------------------
    // Unified component read path
    // -----------------------------------------------------------------------

    /**
     *  @brief  Read one component record from the current file position into a
     *          FieldMap, apply any registered migrations, and dispatch to the
     *          appropriate typed deserialiser.
     *
     *  @param  expectedContainer  the container type the caller is currently in
     *                             (used to guard typed deserialisers)
     *  @param  endComponentId     the component ID that signals end-of-container
     *                             (e.g. EVENT_END_COMPONENT)
     *
     *  @return STATUS_CODE_SUCCESS   — component read and dispatched
     *          STATUS_CODE_NOT_FOUND — end-of-container marker encountered
     *          STATUS_CODE_FAILURE   — I/O or integrity error
     */
    StatusCode ReadNextComponent([[maybe_unused]]const ContainerId expectedContainer, const ComponentId endComponentId);

    /**
     *  @brief  Read a component header and all tagged fields into a FieldMap.
     *          Also reads and verifies the COMPONENT_END_MARKER.
     *
     *  @param  componentId    receives the component type read from the file
     *  @param  schemaVersion  receives the schema version read from the file
     *  @param  fields         receives all tagged field data
     */
    StatusCode ReadComponentFields(ComponentId &componentId, unsigned int &schemaVersion, FieldMap &fields);

    /**
     *  @brief  Apply the registered migration chain to bring fields from
     *          fileSchemaVersion up to the current schema version.
     */
    void ApplyMigrations(const ComponentId componentId, const unsigned int fileSchemaVersion, FieldMap &fields) const;

    // -----------------------------------------------------------------------
    // Typed deserialisers — each receives a fully-populated (and migrated)
    // FieldMap; no checkComponentId parameter, no positional ReadVariable calls.
    // -----------------------------------------------------------------------
    StatusCode ReadVersion([[maybe_unused]] const FieldMap &fields);
    StatusCode ReadMetadata(const FieldMap &fields);
    StatusCode ReadSchemaRegistry(const FieldMap &fields);

    StatusCode ReadSubDetector(const FieldMap &fields);
    StatusCode ReadLArTPC(const FieldMap &fields);
    StatusCode ReadLineGap(const FieldMap &fields);
    StatusCode ReadBoxGap(const FieldMap &fields);
    StatusCode ReadConcentricGap(const FieldMap &fields);

    StatusCode ReadCaloHit(const FieldMap &fields);
    StatusCode ReadTrack(const FieldMap &fields);
    StatusCode ReadMCParticle(const FieldMap &fields);
    StatusCode ReadRelationship(const FieldMap &fields);
    StatusCode ReadEventInformation(const FieldMap &fields);

    // -----------------------------------------------------------------------
    // Migration table
    // -----------------------------------------------------------------------
    struct MigrationKey
    {
        ComponentId  m_componentId;
        unsigned int m_fromVersion;
        bool operator==(const MigrationKey &rhs) const
        {
            return m_componentId == rhs.m_componentId && m_fromVersion == rhs.m_fromVersion;
        }
    };

    struct MigrationKeyHash
    {
        std::size_t operator()(const MigrationKey &k) const
        {
            return std::hash<unsigned int>()(static_cast<unsigned int>(k.m_componentId))
                ^ (std::hash<unsigned int>()(k.m_fromVersion) << 16);
        }
    };

    // -----------------------------------------------------------------------
    // End-of-component sentinel — must match BinaryFileWriter::COMPONENT_END_MARKER
    // -----------------------------------------------------------------------
    static constexpr uint32_t COMPONENT_END_MARKER = 0xDEADBEEFu;

    // -----------------------------------------------------------------------
    // Data members
    // -----------------------------------------------------------------------
    std::ifstream::pos_type m_containerPosition; ///< Start of current container (for GoTo navigation)
    std::ifstream::pos_type m_containerSize;     ///< Byte size of the current container
    std::ifstream           m_fileStream;        ///< Input stream

    std::unordered_map<MigrationKey, MigrationFn, MigrationKeyHash> m_migrations; ///< Registered schema migrations
};

//------------------------------------------------------------------------------------------------------------------------------------------
// ReadVariable template implementations
// These mirror BinaryFileWriter::WriteVariable exactly and are kept for
// ObjectFactory::Read backward compatibility.
//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
inline StatusCode BinaryFileReader::ReadVariable(T &t)
{
    char *const pMemBlock = new char[sizeof(T)];
    m_fileStream.read(pMemBlock, sizeof(T));
    t = *(reinterpret_cast<T *>(pMemBlock));
    delete[] pMemBlock;

    if (!m_fileStream.good())
        return STATUS_CODE_FAILURE;

    return STATUS_CODE_SUCCESS;
}

template <>
inline StatusCode BinaryFileReader::ReadVariable(std::string &t)
{
    uint32_t stringSize = 0;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->ReadVariable(stringSize));

    char *const pMemBlock = new char[stringSize];
    m_fileStream.read(pMemBlock, stringSize);
    t = std::string(pMemBlock, stringSize);
    delete[] pMemBlock;

    if (!m_fileStream.good())
        return STATUS_CODE_FAILURE;

    return STATUS_CODE_SUCCESS;
}

template <>
inline StatusCode BinaryFileReader::ReadVariable(CartesianVector &t)
{
    float x(0.f), y(0.f), z(0.f);
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->ReadVariable(x));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->ReadVariable(y));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->ReadVariable(z));
    t = CartesianVector(x, y, z);
    return STATUS_CODE_SUCCESS;
}

template <>
inline StatusCode BinaryFileReader::ReadVariable(TrackState &t)
{
    CartesianVector position(0.f, 0.f, 0.f), momentum(0.f, 0.f, 0.f);
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->ReadVariable(position));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->ReadVariable(momentum));
    t = TrackState(position, momentum);
    return STATUS_CODE_SUCCESS;
}

} // namespace pandora

#endif // #ifndef PANDORA_BINARY_FILE_READER_H
