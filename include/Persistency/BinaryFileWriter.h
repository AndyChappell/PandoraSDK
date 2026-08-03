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

#include "Persistency/FileWriter.h"
#include "Persistency/Persistency.h"

#include <fstream>

namespace pandora
{

/**
 *  @brief  BinaryFileWriter
 *
 *  Writes Pandora objects to a compact binary file using tagged-field component
 *  records. Wire layout per component:
 *
 *      [ComponentId  : uint32]
 *      [SchemaVersion: uint32]
 *      [NumFields    : uint32]
 *      repeated NumFields times:
 *          [TagLength : uint16]
 *          [Tag       : char * TagLength]
 *          [DataLength: uint32]
 *          [Data      : byte * DataLength]
 *      [END_MARKER   : uint32 = 0xDEADBEEF]
 */
class BinaryFileWriter : public FileWriter
{
public:
    /**
     *  @brief  Constructor
     *
     *  @param  pandora   the pandora instance to be used alongside the file writer
     *  @param  fileName  the name of the output file
     *  @param  fileMode  APPEND (default) or OVERWRITE
     */
    BinaryFileWriter(const pandora::Pandora &pandora, const std::string &fileName,
        const FileMode fileMode = APPEND);

    /**
     *  @brief  Destructor
     */
    ~BinaryFileWriter();

    StatusCode WriteGlobalHeader();

private:
    StatusCode WriteHeader(const ContainerId containerId);
    StatusCode WriteFooter();
    StatusCode WriteMetadata();
    StatusCode WriteSchemaRegistry();
    StatusCode WriteSubDetector(const SubDetector *const pSubDetector);
    StatusCode WriteLArTPC(const LArTPC *const pLArTPC);
    StatusCode WriteDetectorGap(const DetectorGap *const pDetectorGap);
    StatusCode WriteCaloHit(const CaloHit *const pCaloHit);
    StatusCode WriteTrack(const Track *const pTrack);
    StatusCode WriteMCParticle(const MCParticle *const pMCParticle);
    StatusCode WriteRelationship(const RelationshipId relationshipId,
        const void *address1, const void *address2, const float weight);
    StatusCode WriteEventInformation();

    StatusCode WriteComponent(const ComponentId componentId,
        const unsigned int schemaVersion, const FieldMap &fields);

    // Low-level stream primitive used by WriteComponent internals only
    template <typename T>
    StatusCode WriteVariable(const T &t);

    static constexpr uint32_t COMPONENT_END_MARKER = 0xDEADBEEFu;

    std::ofstream::pos_type m_containerPosition;
    std::ofstream           m_fileStream;
};

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
inline StatusCode BinaryFileWriter::WriteVariable(const T &t)
{
    m_fileStream.write(reinterpret_cast<const char *>(&t), sizeof(T));
    if (!m_fileStream.good()) return STATUS_CODE_FAILURE;
    return STATUS_CODE_SUCCESS;
}

template <>
inline StatusCode BinaryFileWriter::WriteVariable(const std::string &t)
{
    const uint32_t stringSize(static_cast<uint32_t>(t.size()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(stringSize));
    m_fileStream.write(t.c_str(), stringSize);
    if (!m_fileStream.good()) return STATUS_CODE_FAILURE;
    return STATUS_CODE_SUCCESS;
}

} // namespace pandora

#endif // #ifndef PANDORA_BINARY_FILE_WRITER_H
