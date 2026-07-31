/**
 *  @file   PandoraSDK/include/Persistency/XmlFileWriter.h
 *
 *  @brief  Header file for the xml file writer class.
 *
 *  $Log: $
 */
#ifndef PANDORA_XML_FILE_WRITER_H
#define PANDORA_XML_FILE_WRITER_H 1

#include "Pandora/Pandora.h"

#include "Persistency/FileWriter.h"
#include "Persistency/Persistency.h"

#include "Xml/tinyxml.h"

namespace pandora
{

/**
 *  @brief  XmlFileWriter
 *
 *  Writes Pandora objects to a self-describing XML file. Document structure:
 *
 *  <PandoraFile>
 *    <Header>
 *      <Metadata schemaVersion="1">...</Metadata>
 *      <SchemaRegistry schemaVersion="0">...</SchemaRegistry>
 *    </Header>
 *    <Geometry><LArTPC schemaVersion="1">...</LArTPC></Geometry>
 *    <Event><CaloHit schemaVersion="1">...</CaloHit></Event>
 *  </PandoraFile>
 */
class XmlFileWriter : public FileWriter
{
public:
    /**
     *  @brief  Constructor
     *
     *  @param  pandora   the pandora instance
     *  @param  fileName  the name of the output file
     *  @param  fileMode  APPEND (default) or OVERWRITE
     */
    XmlFileWriter(const pandora::Pandora &pandora, const std::string &fileName,
        const FileMode fileMode = APPEND);

    /**
     *  @brief  Destructor — saves the XML document to disk
     */
    ~XmlFileWriter();

    StatusCode WriteGlobalHeader();

private:
    StatusCode WriteHeader(const ContainerId containerId);
    StatusCode WriteFooter();
    StatusCode WriteMetadata();
    StatusCode WriteSchemaRegistry();
    StatusCode WriteComponent(const std::string &elementName,
        const unsigned int schemaVersion, const FieldMap &fields);
    StatusCode WriteSubDetector(const SubDetector *const pSubDetector);
    StatusCode WriteLArTPC(const LArTPC *const pLArTPC);
    StatusCode WriteDetectorGap(const DetectorGap *const pDetectorGap);
    StatusCode WriteCaloHit(const CaloHit *const pCaloHit);
    StatusCode WriteTrack(const Track *const pTrack);
    StatusCode WriteMCParticle(const MCParticle *const pMCParticle);
    StatusCode WriteRelationship(const RelationshipId relationshipId,
        const void *address1, const void *address2, const float weight);
    StatusCode WriteEventInformation();

    static unsigned int GetSchemaVersion(const ComponentId componentId);

    TiXmlDocument *m_pXmlDocument;
    TiXmlElement  *m_pContainerXmlElement;
    TiXmlElement  *m_pCurrentXmlElement;
};

} // namespace pandora

#endif // #ifndef PANDORA_XML_FILE_WRITER_H
