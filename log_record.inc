<?php

/*
 * log_record.inc
 * 
 * The log_record_add( ) function below was developed largely from
 * https://www.drupal.org/node/1388922.
 * 
 * And log_record_fetch( ) is from https://www.drupal.org/node/1079616.
 * 
 */

/**
 * Add or replace one record of type '$type' in the database.  $record should
 * be no more than 255 characters in length and will be trimmed before inserting.
 *
 * @param $rec
 * @param $sort
 * @param $type
 * @param $record
 */
function log_record_add($rec, $sort, $type, $record) {
  $pid = $type.':'.$rec.'.'.$sort;
  if (strlen($record) > 0) {
    lr_set($pid, $record);
  } else {
    lr_clear($pid);
  }
}

/**
 * Attention!  This function is no longer supported.
 * Use MySQL to delete tables from the lr_records database instead.
 *
 * Remove all log_records, except the $type specified, from the database.
 *
 * To quickly delete ALL log_record nodes, use drush from the command line...
 *   drush delete-all log_record
 * 
 * @param type $type
 * @return type
 */
function log_record_clear($type='None') {
  return;
}

/*
 * Fetch all log_records of type '$type' from the database.  
 *
 * Note: If $type is prefixed by 'key-' then an associative array is returned
 * with pairs of the form $key => $data.  If $type does not have this
 * prefix, then all records of type '$type' are returned in a
 * non-associative array sorted by key (primary) and sort (secondary). 
 * 
 * If $type is not 'key-*' and the $carriage_control argument is not FALSE, 
 * or is not specified, the fetch will return each record with a trailing 
 * new-line character for printing.
 * 
 * @param string $type
 *   The log_record type to be fetched.
 * @param boolean $carriage_control
 *   If TRUE, then a newline character is appended to the end of each non-key record.
 * @return string
 */
function log_record_fetch($type, $carriage_control=TRUE) {
  $pid = $type.':*';
  $r = lr_get($pid);
  return $r;
}

/**
 * Fetch $key $type log_records and return an array of $sort => $value pairs.
 * 
 * @param type $key
 * @param type $type
 * @return boolean
 *
function log_record_fetch_associative( $key, $type ) {
  $r = array( );
  $query = new EntityFieldQuery( );
  $string = "$key";
  
  // Get the specified type only and only for matching $keys.
  $result = $query->entityCondition( 'entity_type', 'node' )
    ->entityCondition( 'bundle', 'log_record' )
    ->fieldCondition( 'field_record_type', 'value', $type, '=' )
    ->fieldCondition( 'field_object_key', 'value', $string, '=' )
    ->fieldOrderBy( 'field_object_sort', 'value' )
    ->execute( );
  
  $nodes = node_load_multiple( array_keys( $result['node'] ));
  
  foreach ( $nodes as $n ) {
    $r[ $n->field_object_sort['und'][0]['value'] ] = $n->title;
  }
  
  if ( count( $r ) < 1 ) {
    return FALSE;
  } else {
    return $r;
  }
}

function log_record_found( $key, $sort, $type, $removeDuplicates=TRUE ) {
/* -----------------------------------------------------------------------------
 * Return the NID of '$type' log_record with key of $key and sort of $sort. 
 * Returns FALSE if none found.  This function also removes all duplicate 
 * log_record nodes by default.
 *
  $query = new EntityFieldQuery( );
  $s1 = "$key";  
  $s2 = "$sort";
  
  // Get the specified type only and only for matching $keys.
  $result = $query->entityCondition( 'entity_type', 'node' )
    ->entityCondition( 'bundle', 'log_record' )
    ->fieldCondition( 'field_record_type', 'value', $type, '=' )
    ->fieldCondition( 'field_object_key', 'value', $s1, '=' )
    ->fieldCondition( 'field_object_sort', 'value', $s2, '=' )
    ->execute( );
  
  if ( !$result ) {
    return FALSE;
  } else {
    $c = 0;
    foreach( $result['node'] as $nid => $node ) {
      if ( $c === 0 ) {  
        $theNID = $nid;        // save the first NID to return
      } else if ( $removeDuplicates ) {
        node_delete( $nid );   // remove all but the first!
      }
      $c++;
    }
  }
  return $theNID;
}



/*
 * object_record.inc converted to work for new log_record types.
 *
 * A safe MySQL DB class and wrapper functions for simple data storage and retrieval.
 *
 */

/**
 * Connect to the log_records database.
 *
 * @return SafeMySQL
 *   The opened log_records database.
 */
function lr_connect( ) {
  static $lrDB;
  if (!$lrDB) {
    $opts = array(
      'user'    => 'digital',
      'pass'    => 'Grinne11',
      'db'      => 'log_records',
      'charset' => 'utf8'
    );
    $lrDB = new SafeMySQL($opts);
  }
  return $lrDB;
}

/**
 * Make an lr_$namespace table in the log_records database.
 *
 * @param SafeMySQL $db
 *   The opened database.
 * @param string $namespace
 *   The PID namespace to name the table for.
 * @return string $table
 *   The name of the table.
 */
function lr_make_table($db, $namespace) {

  $table = 'lr_'.$namespace;
  $q = "SHOW TABLES LIKE '$table'";
  $result = $db->query($q);

  // Create the table with columns 'rec', 'sort', and 'text'.

  if ($db->numRows($result) < 1) {            // Do this only once if necessary!
    $create = "CREATE TABLE IF NOT EXISTS ?n (rec INT NOT NULL, sort INT NOT NULL, text varchar(255), PRIMARY KEY(rec, sort))";
    $result = $db->query($create, $table);
  }

  return $table;
}

/**
 * Set (insert or update as needed) one record of data for the specified $pid.
 *
 * @param string $pid
 *   The $namespace:$rec.$sort value for which the data is to be stored.
 * @param string $record
 *   The returned text of the record.
 */
function lr_set($pid, $record) {
  list($ns, $x) = explode(':', $pid);
  list($rec, $sort) = explode('.', $x);
  $db = lr_connect( );
  $table = lr_make_table($db, $ns);
  $sql = "INSERT INTO ?n SET rec=?i, sort=?i, text=?s ON DUPLICATE KEY UPDATE text=?s";
  $db->query($sql, $table, $rec, $sort, $record, $record);
}

/**
 * Clear (delete) all records for the specified $pid.
 *
 * @param string $pid
 *   The $namespace:$rec.$sort value for which the data is to be deleted.
 */
function lr_clear($pid) {
  list($ns, $x) = explode(':', $pid);
  list($rec, $sort) = explode('.', $x);
  $db = lr_connect( );
  $table = lr_make_table($db, $ns);

  if ($sort == '*') {
    $sql = "DELETE FROM ?n WHERE (rec=?i);";
    $db->query($sql, $table, $rec);
  } else {
    $sql = "DELETE FROM ?n WHERE (rec=?i AND sort=?i);";
    $db->query($sql, $table, $rec, $sort);
  }
}

/**
 * Get one or all (:*) records of data for the specified target.
 *
 * @param string $pid
 *   The $namespace:$rec.$sort value for which the data is to be retrieved.
 * @return
 */
function lr_get($pid) {
  list($ns, $pidn) = explode(':', $pid);
  $db = lr_connect();
  $table = lr_make_table($db, $ns);

  if ($pidn != '*') {
    list($rec, $sort) = explode('.', $pidn);
    $sql = "SELECT * FROM ?n WHERE rec=?i, sort=?i";
    $record = $db->getAll($sql, $table, $rec, $sort);
    if (empty($record)) {
      return FALSE;
    } else {
      return $record[0];
    }

  } else {
    $sql = "SELECT * FROM ?n";
    $record = $db->getAll($sql, $table);
    if (empty($record)) {
      return FALSE;
    } else {
      return $record;
    }
  }
}

/**
 * Delete one data record for the specified $namespace:$rec.$sort target.
 *
 * @param string $pid
 *   The $namespace:$rec.$sort ID for which the data is to be removed.
 */
function lr_delete($pid) {
  list($ns, $pidn) = explode(':', $pid);
  list($rec, $sort) = explode('.', $pidn);
  $db = lr_connect( );
  $table = lr_make_table($db, $ns);
  $sql = "DELETE FROM ?n WHERE rec=?i, sort=?i";
  $db->query($sql, $table, $rec, $sort);
}

