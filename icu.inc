<?php

/**
 * Parent object $object adopts child identified by $newPID.
 *
 * @param $object
 *   The parent (compound) object.
 * @param $newPID
 *   The PID of the child to be adopted.
 * @param $seqNo
 *   The target sequence number of the child.  Defaults to 1.  @TODO...count the kids and increment
 */
function icu_adopt_child($object, $newPID, $seqNo=1) {
  $repo = $object->repository;
  try {
    $child = $repo->getObject($newPID);
  } catch (Exception $e) { die('Caught Fedora exception: '.$e->getMessage()); }

  // Assign the child to the parent.
  $child->relationships->add(FEDORA_RELS_EXT_URI, 'isConstituentOf', $object->id);

  // Give the child its sequence number.
  $parent = "isSequenceNumberOf" . $object->id;
  $mom = str_replace(':', '_', $parent);
  $child->relationships->add(ISLANDORA_RELS_EXT_URI, $mom, $seqNo, TRUE);

  drush_log(__FUNCTION__ . ": Object $newPID has been adopted as child number $seqNo of " . $object->id.".", 'status');
  return;
}

/**
 * @TODO...duplicate of above?
 * Parent $object adopts child identified by $newPID.
 *
 * @param $object
 *   The parent (compound) object.
 * @param $newPID
 *   The PID of the child to be adopted.
 * @param $seqNo
 *   The target sequence number of the child.  Defaults to 1.  @TODO...count the kids and increment
 *
function icu_adopt_child($object, $newPID, $seqNo=1) {
  $repo = $object->repository;
  try {
    $child = $repo->getObject($newPID);
  } catch (Exception $e) { die('Caught Fedora exception: '.$e->getMessage()); }

  // Assign the child to the parent.
  $child->relationships->add(FEDORA_RELS_EXT_URI, 'isConstituentOf', $object->id);

  // Give the child its sequence number.
  $parent = "isSequenceNumberOf" . $object->id;
  $mom = str_replace(':', '_', $parent);
  $child->relationships->add(ISLANDORA_RELS_EXT_URI, $mom, $seqNo, TRUE);

  drush_log(__FUNCTION__ . ": Object $newPID has been adopted as child number $seqNo of " . $object->id.".", 'status');
  return;
} */

/**
 * Change the CModel relationship of an existing Fedora object.
 *
 * @param $object
 *   The open Fedora object to change.
 * @param string $CModel
 *   The intended/target content model of the modified object.  Defaults to islandora:compoundCModel.
 */
function icu_change_object_cmodel($object, $CModel='islandora:compoundCModel') {
  $object->relationships->remove(FEDORA_MODEL_URI, 'hasModel');
  $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', $CModel);
  drush_log(__FUNCTION__.': '.$object->id." was converted to the $CModel content model.", 'status');
  return;
}

/**
 * @TODO...duplicate of above?
 * Change the CModel relationship of an existing Fedora object.
 *
 * @param $object
 *   The open Fedora object to change.
 * @param string $CModel
 *   The intended/target content model of the modified object.  Defaults to islandora:compoundCModel.
 *
function icu_change_object_cmodel($object, $CModel='islandora:compoundCModel') {
  $object->relationships->remove(FEDORA_MODEL_URI, 'hasModel');
  $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', $CModel);
  drush_log(__FUNCTION__.': Object '.$object->id." has been converted to use the $CModel content model.", 'status');
  return;
} */

/**
 * Open a connection to the Fedora repository as the identified Drupal user.
 */
function icu_Connect($fedora) {
  global $user;

  // Open the repository as the identified Drupal user.
  if (!$connection = new RepositoryConnection($fedora, $user->name, $user->pass)) {
    drush_log(__FUNCTION__." error: Unable to verify a repository connection!", 'error');
    return FALSE;
  }

  $connection->reuseConnection = TRUE;
  $repository = new FedoraRepository(new FedoraApi($connection), new SimpleCache());

  variable_set(__FUNCTION__, $fedora);     // and also the database variable
  drush_log(__FUNCTION__.": Connection to '$fedora' as '{$repository->ri->connection->username}' is complete.", 'status');
  return $repository;
}

/**
 * Make a new copy of an existing Fedora object.
 *
 * This function copies an existing object as-is, maintaining the same namespace, content,
 * CModel and collection as the original.
 *
 * @param FedoraObject $object
 *   The original object to be copied. The new copy becomes part of the same namespace and
 *   collection as $object.
 * @param string $new_label
 *   The new label for the object.  If NULL then $object->label will be preserved.
 * @return string
 *   The new, ingested object's PID.
 */
function icu_copy_object($object, $new_label=NULL) {
  $repo = $object->repository;
  $pid = $object->id;
  list($ns, $pidn) = explode(':', $pid);

  // Create a whole new object...
  $new_object = $repo->constructObject($ns);   // construct a new object in the target namespace
  $new_object->models = $object->models;       // associate it with $object's content models.
  $new_object->label = (is_null($new_label) ? $object->label : $new_label);   // use the label provided or the old DSID as new label/title
  $new_object->owner = $object->owner;

  // Iterate over all of $object's datastreams and copy each one to the new object.
  // Note that this will NOT produce a valid RELS-EXT, that needs to be done properly below.
  foreach ($object as $ds) {
    $dsid = $ds->id;

    // Skip the RELS-EXT.
    if ($dsid !== 'RELS-EXT') {
      $label = $ds->label;
      $mime = $ds->mimetype;
      $copy = $new_object->constructDatastream($dsid);

      // At this point, remove ALL MODS or DC elements with references to the old PID.
      if ($dsid === 'MODS' || $dsid === 'DC') {
        $content = $ds->content;
        $xml = new DOMDocument();
        $xml->loadXML($content);
        $q = "//*[contains(., '$pid')]";  // all elements with a value containing the old PID...this will include the root!
        $xpath = new DOMXPath($xml);
        $nodes = $xpath->query($q);

        if ($nodes) {
          $nodesToRemove = array( );
          $i = 0;
          foreach ($nodes as $node) {
            if ($i++ > 0) { $nodesToRemove[] = $node; }  // don't remove the root!
          }

          foreach ($nodesToRemove as $node) {
            $node->parentNode->removeChild($node);
          }
        }

        $content = $xml->saveXML( );
        $copy->setContentFromString($content);

      } else {  // Not MODS or DC... save the old datastream content to a temporary file (because some datastreams are BIG) and back to $copy from there.

        $temp_file = drupal_tempnam('temporary://', 'datastream');  // open a temp file
        $object[$dsid]->getContent($temp_file);  // copy DS binary content to the temp file
        $copy->setContentFromFile($temp_file);   // set new DS content from the temp file
        drupal_unlink($temp_file);               // discard the temp file
      }

      // Done with this datastream, ingest it into the new object.
      $copy->label = $label;
      $copy->mimeType = $mime;
      $new_object->ingestDatastream($copy);
    }
  }

  // All done with datastreams, ingest the new object.
  // Ingest the new object into the repository.
  $newPID = $new_object->id;
  try {
    $repo->ingestObject($new_object);
  } catch (Exception $e) { die('Caught Fedora exception: '.$e->getMessage()); }

  // Now, open the new object again and add the missing collection membership(s).
  $obj = $repo->getObject($newPID);
  $collections = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOfCollection');
  foreach ($collections as $collection) {
    $obj->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection['object']['value']);
  }

  // And do the same for any isManageableBy... relations, but make them LITERAL!
  $islandora_rels = $object->relationships->get(ISLANDORA_RELS_EXT_URI);
  foreach ($islandora_rels as $rel) {
    $obj->relationships->add(ISLANDORA_RELS_EXT_URI, $rel['predicate']['value'], $rel['object']['value'], TRUE);
  }

  drush_log(__FUNCTION__.": New object '$newPID' has been created.", 'status');
  return $newPID;
}

/**
 * @TODO...duplicate of above?
 * Make a new copy of an existing Fedora object.
 *
 * This function copies an existing object as-is, maintaining the same namespace, content,
 * CModel and collection as the original.
 *
 * @param FedoraObject $object
 *   The original object to be copied. The new copy becomes part of the same namespace and
 *   collection as $object.
 * @param string $new_label
 *   The new label for the object.  If NULL then $object->label will be preserved.
 * @return string
 *   The new, ingested object's PID.
 *
function icu_copy_object($object, $new_label=NULL) {
  $repo = $object->repository;
  $pid = $object->id;
  list($ns, $pidn) = explode(':', $pid);

  // Create a whole new object...
  $new_object = $repo->constructObject($ns);   // construct a new object in the target namespace
  $new_object->models = $object->models;       // associate it with $object's content models.
  $new_object->label = (is_null($new_label) ? $object->label : $new_label);   // use the label provided or the old DSID as new label/title
  $new_object->owner = $object->owner;

  // Iterate over all of $object's datastreams and copy each one to the new object.
  // Note that this will NOT produce a valid RELS-EXT, that needs to be done properly below.
  foreach ($object as $ds) {
    $dsid = $ds->id;

    // Skip the RELS-EXT.
    if ($dsid !== 'RELS-EXT') {
      $label = $ds->label;
      $mime = $ds->mimetype;
      $copy = $new_object->constructDatastream($dsid);

      // At this point, remove ALL MODS or DC elements with references to the old PID.
      if ($dsid === 'MODS' || $dsid === 'DC') {
        $content = $ds->content;
        $xml = new DOMDocument();
        $xml->loadXML($content);
        $q = "//*[contains(., '$pid')]";  // all elements with a value containing the old PID...this will include the root!
        $xpath = new DOMXPath($xml);
        $nodes = $xpath->query($q);

        if ($nodes) {
          $nodesToRemove = array( );
          $i = 0;
          foreach ($nodes as $node) {
            if ($i++ > 0) { $nodesToRemove[] = $node; }  // don't remove the root!
          }

          foreach ($nodesToRemove as $node) {
            $node->parentNode->removeChild($node);
          }
        }

        $content = $xml->saveXML( );
        $copy->setContentFromString($content);

      } else {  // Not MODS or DC... save the old datastream content to a temporary file (because some datastreams are BIG) and back to $copy from there.

        $temp_file = drupal_tempnam('temporary://', 'datastream');  // open a temp file
        $object[$dsid]->getContent($temp_file);  // copy DS binary content to the temp file
        $copy->setContentFromFile($temp_file);   // set new DS content from the temp file
        drupal_unlink($temp_file);               // discard the temp file
      }

      // Done with this datastream, ingest it into the new object.
      $copy->label = $label;
      $copy->mimeType = $mime;
      $new_object->ingestDatastream($copy);
    }
  }

  // All done with datastreams, ingest the new object.
  // Ingest the new object into the repository.
  $newPID = $new_object->id;
  try {
    $repo->ingestObject($new_object);
  } catch (Exception $e) { die('Caught Fedora exception: '.$e->getMessage()); }

  // Now, open the new object again and add the missing collection membership(s).
  $obj = $repo->getObject($newPID);
  $collections = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOfCollection');
  foreach ($collections as $collection) {
    $obj->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection['object']['value']);
  }

  // And do the same for any isManageableBy... relations, but make them LITERAL!
  $islandora_rels = $object->relationships->get(ISLANDORA_RELS_EXT_URI);
  foreach ($islandora_rels as $rel) {
    $obj->relationships->add(ISLANDORA_RELS_EXT_URI, $rel['predicate']['value'], $rel['object']['value'], TRUE);
  }

  drush_log(__FUNCTION__.": New object '$newPID' has been created.", 'status');
  return $newPID;
} */

/**
 * Returns the DSIDs for known, not-original-content datastreams.
 *
 * @return array
 */
function icu_DSIDs( ) {
  return array_keys(rcDSMimes( ));
}

/**
 * Returns an array of MIME types for known, non-original content datastreams.
 *
 * @return array
 */
function icu_DSMimes( ) {
  static $ds = array( 'RELS-EXT' => 'application/rdf+xml',
    'DC' => 'text/xml',
    'MODS' => 'text/xml',
    'RELS-INT' => 'application/rdf+xml',
    'TN' => 'image/jpeg',
    'POLICY' => 'text/xml',
    'PREVIEW' => 'image/jpeg',
    'JPG' => 'image/jpeg',
    'JP2' => 'image/jp2',
    'MEDIUM_SIZE' => 'image/jpeg',
    'FULL_TEXT' => 'text/plain',
    'OCR' => 'text/plain',
    'RAW_HOCR' => 'text/xml',
    'HOCR' => 'text/html',
    'ENCODED_OCR' => 'text/xml',
    'islandora_workflow' => 'text/xml',
    'OriginalDC' => 'text/xml',
    'PROXY_MP3' => 'audio/mpeg',
    'MP4' => 'video/mp4',
  );
  return $ds;
}

/**
 * A simple routine to pluralize a noun based on $count.
 *
 * @param integer $count - The number of objects.
 * @param $singular - The singular noun to return.
 * @param bool|FALSE $plural - The plural noun to return, or simply append an 's' to the $singular if FALSE.
 * @return bool|string - The pluralized noun.
 *
 */
function icu_pluralize($count, $singular, $plural = false) {
  return ($count == 1 ? $singular : ($plural ? $plural : $singular.'s'));
}

/**
 * Fetch a list of ALL numbered PIDs in a specified namespace.
 *
 * @param $namespace
 *   The namespace of the PIDs to return.
 * @param $start
 *   The beginning PID value (namespace:number).
 * @param null|int $end
 *   The end PID number ONLY.  Defaults to NULL which returns
 *   all available PIDs after $start.
 * @param null|string $onlyAfter
 *   Specifies the 'fgs_lastModifiedDate_dt' date/time for the query to filter objects with.  Defaults to
 *   the start of the millennium (returns ALL objects).
 * @return array|bool
 *   FALSE upon error, or an array of PIDs.
 *
 */
function icu_fetch_PIDs($namespace, $start, $end=NULL, $onlyAfter="2000-01-01T00:00:00Z", $collection=NULL, $contentType=NULL, $created=NULL) {
  $pids = $pidNums = array( );

  // Check the $namespace and $start arguments.
  list($ns, $startNum) = explode(':', $start);
  $startN = intval($startNum);
  if ($startN < 1 || $namespace != $ns) {
    drush_log(__FUNCTION__." error.  Invalid namespace ($namespace) or start ($start) specified.", 'error');
    return FALSE;
  }

  // The following block of code builds an HTTP Solr request.  As such, it
  // does NOT adhere to user permissions per XACML.  It was designed to
  // avoid possible recursion.

  // Build the namespace object query...
  $q = "PID:(*".$namespace."\:*)";

  $fq = "&fq=fgs_lastModifiedDate_dt:[".$onlyAfter."%20TO%20NOW]";    // filter by modified date
  if ($collection) { $fq .= "&fq=RELS_EXT_isMemberOfCollection_uri_s:".$collection; }
  if ($contentType) { $fq .= "&fq=RELS_EXT_hasModel_uri_s:".$contentType; }
  if ($created) { $fq .= "&fq=fgs_createdDate_dt:[".$created."%20TO%20NOW]"; }   // filter by created date

  $url = variable_get('islandora_solr_url', 'localhost:8080/solr');
  $fl = 'PID';
  if (preg_match('/^http:\/\/\w.*/',$url) == 1) {
    $request = $url . '/select?q=' . $q . $fq . '&fl=' . $fl . '&wt=php&rows=99999';
  } else {
    $request = 'http://' . $url . '/select?q=' . $q . $fq . '&fl=' . $fl . '&wt=php&rows=99999';
  }

  // Build and execute the Solr query.
  // Solr query code from http://wiki.apache.org/solr/SolPHP
  //
  //
  $code = file_get_contents($request);
  if ($code) {
    eval("\$result = " . $code . ";");
    if ($result) {
      $o = $result['response']['docs'];
      foreach ($o as $a) {
        $pid = $a['PID'];
        list($ns, $pidn) = explode(':',$pid);
        $pidN = intval($pidn);
        if ($pidN >= $startN && (is_null($end) || $pidN <= $end)) { $pidNums[] = $pidN; }  // save only numeric PID numbers
      }
    }
    asort($pidNums);                   // sort by PID number
    foreach ($pidNums as $pidN) {
      $pids[] = $ns.':'.$pidN;         // return PIDs in sorted order
    }
    return $pids;
  }
  return FALSE;
}

/**
 * Return the proper MIME type for an OBJ datastream based on content model.  Some have * because they can vary.
 *
 * @return mixed
 */
function icu_OBJMimes( ) {
  static $ds = array( 'sp_pdf' => 'application/pdf',
    'sp_basic_image' => 'image/jpeg',
    'sp_large_image_cmodel' => 'image/tiff',
    'compoundCModel' => 'NONE',
    'binaryObjectCModel' => 'application/*',
    'bookCModel' => 'image/tiff',
    'pageCModel' => 'image/tiff',
    'sp-audioCModel' => 'audio/*',
    'sp_videoCModel' => 'video/*',
  );
  return $ds;
}

/**
 * Fetch the existing Fedora connection.
 */
function icu_Repository( ) {

  // Check that the repository is open.  If not, open it.
  if (isset($_SESSION[__FUNCTION__])) {
    $repository = $_SESSION[__FUNCTION__];
    return $repository;
  }

  // If not, open it.
  if (!$fedora = variable_get('icu_Connect', NULL)) {
    drush_log(__FUNCTION__." error: No Fedora connection or URL found!  Use 'drush icuC' to make a connection.", 'error');
    die;
  } else {
    $repository = icu_Connect($fedora);
    $_SESSION[__FUNCTION__] = $repository;
  }

  return $repository;
}

/**
 * Build and execute a Solr query as an HTTP solr request.
 *
 * @param $q
 *   The query to execute in a form like... "RELS_EXT_hasModel_uri_s:(info\:fedora/islandora\:collectionCModel)"
 * @param $fl
 *   The CSV list of solr fields to return like... "PID,fgs_label_s"
 *
 * @return bool|array
 *   FALSE upon error, or the resulting array of requested field arrays.  So, a
 *   $q of "PID,fgs_labels" might return an array like this...
 *      [0] => array("PID" => "faulconer-art:4", "fgs_label_s" => "Heat"),
 *      [1] => array("PID" => "faulconer-art:1234", "fgs_label_s" => "Bogus Object"),
 *      ...
 *
 */
function icu_solr_query($q, $fl) {

  // The following block of code builds an HTTP Solr request.  As such, it
  // does NOT adhere to user permissions per XACML.  It was designed to
  // avoid possible recursion with Islandora's solr queries.

  // Build the collections list query...

  $url = variable_get('islandora_solr_url', 'localhost:8080/solr');
  $request = 'http://' . $url . '/select?q=' . $q . '&fl=' . $fl . '&wt=php&rows=999';

  // Build and execute a Solr query for basic collections.
  //
  // !!! Important !!!
  //   This cannot be an IslandoraSolrQueryProcessor( ) instance.  Doing that
  //   is likely to introduce recursion in the islandora_solr_search hooks!
  //
  // Solr query code from http://wiki.apache.org/solr/SolPHP
  //
  $code = file_get_contents($request);
  if ($code) {
    eval("\$result = " . $code . ";");
    if ($result) {
      $found = (isset($result['response']['numFound']) ? $result['response']['numFound'] : FALSE);
      if ($found) {
        $c = $result['response']['docs'];
        return $c;
      }
    }
  }

  return FALSE;
}

/**
 * Return the count and content from an existing health check cell.
 *
 * @param string $cell
 * @return array
 */
function icu_get_count_and_content($cell) {
  $matches = array( );
  if (empty($cell)) {
    return array(0, NULL);
  } else if (preg_match('/^\[(\d+)] (.+)$/', $cell, $matches)) {  // count and content
    return array(intval($matches[1]), trim($matches[2]));
  } else if (preg_match('/^\[(\d+)]$/', $cell, $matches)) {  // count only
    return array(intval($matches[1]), NULL);
  } else if (preg_match('/^(.*)$/', $cell, $matches)) {  // content only
    $content = $matches[1];
    return array(1, trim($content));
  }
}

/*
@TODO... Fix this!
*/

//do this until we expose these in a module or library
@include_once 'sites/all/libraries/tuque/Datastream.php';
@include_once 'sites/all/libraries/tuque/FedoraApi.php';
@include_once 'sites/all/libraries/tuque/FedoraApiSerializer.php';
@include_once 'sites/all/libraries/tuque/Object.php';
@include_once 'sites/all/libraries/tuque/RepositoryConnection.php';
@include_once 'sites/all/libraries/tuque/Cache.php';
@include_once 'sites/all/libraries/tuque/RepositoryException.php';
@include_once 'sites/all/libraries/tuque/Repository.php';
@include_once 'sites/all/libraries/tuque/FedoraRelationships.php';
