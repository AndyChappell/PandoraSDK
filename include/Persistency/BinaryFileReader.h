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

#include "Persistency/FileReader.h"
#include "Persistency/Persistency.h"

#include <fstream>
#include <functional>
#include <unordered_map>

namespace pandora
{

/**
 *  @brief  BinaryFileReader
 *
 *  Reads Pandora objects from a binary file written by BinaryFileWriter.
 *
 *  Migration registration
 *  ----------------------
 *  Call RegisterMigration() to install a schema upgrade function for a given
 *  component type and version transition. Migrations are chained automatically.
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
 */
class BinaryFileReader : public FileReader
{
public:
    BinaryFileReader(const pandora::Pandora &pandora, const std::string &fileName);
    ~BinaryFileReader();

    typedef std::function<void(FieldMap &)> MigrationFn;

    void RegisterMigration(const ComponentId componentId, const unsigned int fromVersion,
        const unsigned int toVersion, MigrationFn fn);

private:
    StatusCode ReadHeader();
    StatusCode GoToNextContainer();
    ContainerId GetNextContainerId();
    StatusCode GoToGeometry(const unsigned int geometryNumber);
    StatusCode GoToEvent(const unsigned int eventNumber);
    StatusCode ReadNextGlobalHeaderComponent();
    StatusCode ReadNextGeometryComponent();
    StatusCode ReadNextEventComponent();

    StatusCode ReadNextComponent([[maybe_unused]] const ContainerId expectedContainer,
        const ComponentId endComponentId);
    StatusCode ReadComponentFields(ComponentId &componentId, unsigned int &schemaVersion, FieldMap &fields);
    void ApplyMigrations(const ComponentId componentId, const unsigned int fileSchemaVersion, FieldMap &fields) const;

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

    // Low-level stream primitive — used only by ReadComponentFields
    template <typename T>
    StatusCode ReadVariable(T &t);

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

    static constexpr uint32_t COMPONENT_END_MARKER = 0xDEADBEEFu;

    std::ifstream::pos_type m_containerPosition;
    std::ifstream::pos_type m_containerSize;
    std::ifstream           m_fileStream;

    std::unordered_map<MigrationKey, MigrationFn, MigrationKeyHash> m_migrations;
};

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
inline StatusCode BinaryFileReader::ReadVariable(T &t)
{
    char *const pMemBlock = new char[sizeof(T)];
    m_fileStream.read(pMemBlock, sizeof(T));
    t = *(reinterpret_cast<T *>(pMemBlock));
    delete[] pMemBlock;
    if (!m_fileStream.good()) return STATUS_CODE_FAILURE;
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
    if (!m_fileStream.good()) return STATUS_CODE_FAILURE;
    return STATUS_CODE_SUCCESS;
}

} // namespace pandora

#endif // #ifndef PANDORA_BINARY_FILE_READER_H
