/**
 *  @file   PandoraSDK/include/Persistency/FileWriter.h
 *
 *  @brief  Header file for the file writer class.
 *
 *  $Log: $
 */
#ifndef PANDORA_FILE_WRITER_H
#define PANDORA_FILE_WRITER_H 1

#include "Pandora/StatusCodes.h"

#include "Persistency/PandoraIO.h"
#include "Persistency/Persistency.h"

#include <string>

namespace pandora
{

class Pandora;

//------------------------------------------------------------------------------------------------------------------------------------------

/**
 *  @brief  FileWriter class
 */
class FileWriter : public Persistency
{
public:
    /**
     *  @brief  Constructor
     *
     *  @param  pandora   the pandora instance to be used alongside the file writer
     *  @param  fileName  the name of the output file
     */
    FileWriter(const pandora::Pandora &pandora, const std::string &fileName);

    /**
     *  @brief  Destructor
     */
    virtual ~FileWriter();

    /**
     *  @brief  Write the global header to the file. Overridden by concrete subclasses.
     */
    virtual StatusCode WriteGlobalHeader() = 0;

    /**
     *  @brief  Write the current geometry information to the file
     */
    StatusCode WriteGeometry();

    /**
     *  @brief  Write the specified event components to the file.
     *
     *  @param  caloHitList              the list of calo hits to write
     *  @param  trackList                the list of tracks to write
     *  @param  mcParticleList           the list of MC particles to write
     *  @param  writeMCRelationships     whether to write MC relationship information
     *  @param  writeTrackRelationships  whether to write track relationship information
     */
    StatusCode WriteEvent(const CaloHitList &caloHitList, const TrackList &trackList,
        const MCParticleList &mcParticleList, const bool writeMCRelationships = true,
        const bool writeTrackRelationships = true);

protected:
    virtual StatusCode WriteHeader(const ContainerId containerId) = 0;
    virtual StatusCode WriteFooter() = 0;
    virtual StatusCode WriteSubDetector(const SubDetector *const pSubDetector) = 0;
    virtual StatusCode WriteLArTPC(const LArTPC *const pLArTPC) = 0;
    virtual StatusCode WriteDetectorGap(const DetectorGap *const pDetectorGap) = 0;
    virtual StatusCode WriteCaloHit(const CaloHit *const pCaloHit) = 0;
    virtual StatusCode WriteTrack(const Track *const pTrack) = 0;
    virtual StatusCode WriteMCParticle(const MCParticle *const pMCParticle) = 0;
    virtual StatusCode WriteRelationship(const RelationshipId relationshipId,
        const void *address1, const void *address2, const float weight = 1.f) = 0;
    virtual StatusCode WriteEventInformation() = 0;

    /**
     *  @brief  Return the current schema version for a given component type.
     *
     *  Single source of truth shared by every concrete FileWriter (binary, XML,
     *  and any future format), so schema versions cannot drift between formats.
     *  Increment a value here (and register a corresponding reader migration in
     *  BinaryFileReader/XmlFileReader) when a field is removed or its semantics
     *  change. Adding a new optional field does not need a bump.
     */
    static unsigned int GetSchemaVersion(const ComponentId componentId);

    /**
     *  @brief  Populate m_schemaRegistry from GetSchemaVersion() for every
     *          persisted component type. Called once by each concrete writer's
     *          constructor, after the output stream/document is ready.
     */
    void PopulateSchemaRegistry();

private:
    StatusCode WriteSubDetectorList();
    StatusCode WriteLArTPCList();
    StatusCode WriteDetectorGapList();
    StatusCode WriteTrackList(const TrackList &trackList);
    StatusCode WriteCaloHitList(const CaloHitList &caloHitList);
    StatusCode WriteMCParticleList(const MCParticleList &mcParticleList);
    StatusCode WriteCaloHitToMCParticleRelationships(const CaloHitList &caloHitList);
    StatusCode WriteTrackToMCParticleRelationships(const TrackList &trackList);
    StatusCode WriteMCParticleRelationships(const MCParticleList &mcParticleList);
    StatusCode WriteTrackRelationships(const TrackList &trackList);
    StatusCode WriteCaloHitToMCParticleRelationship(const CaloHit *const pCaloHit);
    StatusCode WriteTrackToMCParticleRelationship(const Track *const pTrack);
    StatusCode WriteMCParticleRelationships(const MCParticle *const pMCParticle);
    StatusCode WriteTrackRelationships(const Track *const pTrack);
};

} // namespace pandora

#endif // #ifndef PANDORA_FILE_WRITER_H
