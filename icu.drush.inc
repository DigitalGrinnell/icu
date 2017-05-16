<?php

/**
 * Some available drush functions...
 *
 * drush_log('Log an event using drush', 'warning');
 * drush_set_error('Set an error with drush.');
 * dt('Translate strings with drush');
 * drush_print('Print to command line with drush');
 * drush_print_table($rows, TRUE); //print a command line table with drush
 * drush_confirm('Are you sure you want to continue?', $indent = 0); //Add drush confirmation
 *
 */

/**
 * Implements hook_drush_command().
 */
function icu_drush_command( ) {
  $items = array();

  $items['icuConnect'] = array(
    'description' => t('Used to establish a Tuque connection to a named Fedora repository.'),
    'arguments'   => array(
      'fedora'    => t("The Fedora repository URL of the form http://repository.domain.com:8080/fedora."),
    ),
    'examples' => array(
      'Example' => 'drush -u 55 icuC http://repository.grinnell.edu:8080/fedora',
    ),
    'aliases' => array('icuC'),
  );

  return $items;
}

/**
 * Drush callback function for the 'drush icuC' command.
 *
 * @param string $fedora
 *   The Fedora repository path from the sole argument to the icuC command.
 *
 * @return bool|\FedoraRepository
 */
function drush_icu_icuConnect($fedora) {
  module_load_include('inc', 'icu', 'icu');
  return icu_Connect($fedora);
}

/**
 * Examine common ixxX command arguments/options and return a list of PIDs meeting
 * the specified criteria.
 *
 * @param string $pidSpec
 *   The range (or single) PID argument as specified in the ixxX command.
 * @param int $last
 *   Returns the 'last' PID value based on the range specification, if any.  Note that $last may or may not be an existing object.
 * @param int $pidn
 *   Returns the integer PID number of 'range'.  Note that $pidn may or may not be an existing object.
 * @param int $range
 *   Returns the total number of PIDs to address.
 * @param string $namespace
 *   Returns the namespace (without a colon!) associated with the PID numbers returned by the function.
 * @param null|string $lastOp
 *   The date/time of the last successful operation.  Only objects modified after this date should be processed.
 * @param bool $return
 *   If TRUE, the function returns an array of valid PID numbers ONLY.
 * @return array
 *   An array of valid PIDs from the specified range.
 *
 */
function icu_drush_prep($pidSpec, &$last, &$pidn, &$range, &$namespace, $lastOp='2000-01-01T00:00:00Z', $collection=NULL, $contentType=NULL, $created=NULL) {
  module_load_include('inc', 'icu', 'icu');

  // Initialize some things.
  list($namespace, $pidN) = explode(':', $pidSpec);    // Break $pidSpec into namespace and PID number.
  variable_set('icg_namespace', "$namespace:");

  $local = $lastOp;    // all date/time values are stored in local time
  drush_log(__FUNCTION__." will consider only objects modified after $local.", 'status');

  // If the $pidN is an asterisk, then we are processing only collection objects.
  if ($pidN === '*' && module_exists('islandora_solr_collection_search')) {
    module_load_include('inc', 'islandora_solr_collection_search');
    $pids = islandora_solr_collection_search_get_collection_list( );
    $range = count($pids);
    drush_log("The --collection, --contentType, --repeat, --modAfter and --createdAfter options will be ignored so that all $range collection objects will be processed.", 'status');
    return $pids;
  }

  // If $pidN is not an integer...then we have only one PID to process...do it!
  if (intval($pidN) === 0) {
    drush_log("Completed fetch of a single object PID.", 'status');
    $range = 1;
    return array($pidSpec);
  }

  list($begin, $end) = explode('-', $pidN);   // Break $pidN into beginning and end of range, if necessary
  $pidn = intval($begin);

  // Check for the --repeat option or a defined $end to the range.
  if (!empty($end)) {
    $endN = intval($end);
    $repeat = $endN - intval($begin);
    $range = $repeat + 1;
  } else {
    $end = $endN = NULL;
    $r = drush_get_option('repeat', 'all');
    if ($r != 'all') {
      $repeat = intval($r);
      if ($repeat < 0) {
        drush_log("The --repeat option must be 'all', zero or greater!  Forcing --repeat set to 'all'.", 'warning');
        $repeat = 'all';
      } else {
        $range = $repeat + 1;
        $endN = $begin + $repeat;
      }
    } else {
      $repeat = 'all';
    }
  }

  $start = date('H:i:s');
  list($spec, $rest) = explode('-', $pidSpec);
  drush_log("Starting operation for PID '$spec' and --repeat='$repeat' at $start.", 'status');

  if ($collection) { drush_log("PIDs will be selected only within --collection='$collection'.", 'status'); }
  if ($contentType) { drush_log("PIDs will be selected only where --contentType='$contentType'.", 'status'); }
  if ($created) { drush_log("PIDs will be selected only where --createdAfter since '$created'.", 'status'); }

  // Fetch the specified PIDs and return them.
  drush_log("Fetching all valid object PIDs in the specified range.", 'status');
  $pids = icu_fetch_PIDs($namespace, $pidSpec, $endN, $local, $collection, $contentType, $created);
  $range = count($pids);
  $last = end($pids);
  drush_log("Completed fetch of $range valid object PIDs.", 'status');
  return $pids;
}


/**
 * Prints a Drush progress bar directly to STDOUT.
 *
 * Call with no values (NULLs) for $current and $range to initialize the progress bar.
 *
 * @param string $label
 *   Label to appear above the bar when initialized.
 * @param int|NULL $current
 *   The current counter (from 0 to $range) in the icg_first to icg_last progression.
 * @param int|NULL $range
 *   The total number of steps in the icg_first to icg_last progression.
 */
function icu_drush_print_progress($label, $current=NULL, $range=NULL) {
  static $green = "\033[1;32m";
  static $white = "\033[0;37m";

  if (is_null($current)) {
    $output = $green . "Progress: $label \n";
    print $output;
    return;
  }

  $ratio = ($current+1) / $range;
  $percentage = floor($ratio * 100) . '%';
  $columns = drush_get_context('DRUSH_COLUMNS', 80);
  // Subtract 10 characters for the percentage, brackets, spaces and arrow.
  $progress_columns = $columns - 10;
  // If ratio is 1 (complete), the > becomes a = to make a full bar.
  $arrow = ($ratio < 1) ? '>' : '=';
  // Print a new line if ratio is 1 (complete). Otherwise, use a CR.
  $line_ending = ($ratio < 1) ? "\r" : "\n";

  // Determine the current length of the progress string.
  $current_length = floor($ratio * $progress_columns);
  $progress_string = str_pad('', $current_length, "=");

  $output  = $green . '[';
  $output .= $progress_string . $arrow;
  $output .= str_pad('', $progress_columns - $current_length);
  $output .= ']';
  $output .= str_pad('', 5 - strlen($percentage)) . $percentage;
  $output .= $line_ending . $white;

  print $output;
}

/**
 * Compare two DOMNode objects to see if they 'match'.
 *
 * Matching criteria include $node->nodeName AND $node->nodeType AND nodeValue.
 *
 * @param $node1 DOMNode
 *   First DOMNode to compare.
 * @param $node2 DOMNode
 *   Second DOMNode to compare.
 * @param $fullComparison boolean
 *   If set, the comparison will also check that the nodes have the same parent
 *     and the same attributes.
 */
function icu_drush_nodes_match($node1, $node2, $fullComparison=FALSE) {

  if ($node1->nodeName === $node2->nodeName) {
    if ($node1->nodeType === $node2->nodeType) {
      if ($node1->nodeValue === $node2->nodeValue) {
        $len = strlen(trim((string) $node1->nodeValue));
        if ($len > 0) {
          if (!$fullComparison) { return TRUE; }

          $sameAttr = TRUE;
          if ($node1->hasAttributes() && $node2->hasAttributes()) {
            $attr1 = $attr2 = array( );
            if (count($node1->attributes) != count($node2->attributes)) { $sameAttr = FALSE; }
            if ($sameAttr) {
              foreach ($node1->attributes as $attr) { $attr1[] = $attr; }
              foreach ($node2->attributes as $attr) { $attr2[] = $attr; }
              for ($i=0; $i<count($attr1); $i++) {
                if ($attr1[$i]->name != $attr2[$i]->name) { $sameAttr = FALSE; }
                if ($attr1[$i]->value != $attr2[$i]->value) { $sameAttr = FALSE; }
              }
            }
          }

          if ($node1->parentNode === $node2->parentNode && $sameAttr) { return TRUE; }
        }
      }
    }
  }

  return FALSE;
}

/**
 * Build and array of XPath expressions for the elements of a given datastream.
 *
 * @param $datastream
 */
function icu_drush_build_XPath_array($datastream) {
  $simpleXML = new SimpleXMLElement($datastream->content);  // create a SimpleXMLElement object from the XML document
  $contextElements = $simpleXML->xpath('/mods:mods');  // select the root MODS node
  $contextElement = $contextElements[0];  // grab the first context node
  $xpathList = XMLElementToXPath($contextElement->getName(), $contextElement);

  return $xpathList;

  // foreach ($xpathList as $xpath) { echo $xpath . "\n"; }
}

/**
 * Recursive function that takes a SimpleXMLElement object and returns a
 * list of XPath expressions.
 *
 * @param $currentXPath
 * @param $myXMLElement
 */
function XMLElementToXPath($currentXPath, $myXMLElement) {
  $xpathList = array( );
  $attributes = $myXMLElement->attributes( );  // grab attributes from current root element

  // Process element attributes, if any.
  foreach($attributes as $att_key=>$att_value) {  // process element attributes (if any)
    $xpathList[] = ($currentXPath!=''?$currentXPath.'/':'').'@'. $att_key;  // create XPath for attribute
    $xpathList[] = ($currentXPath!=''?$currentXPath:'').'[@'. $att_key.'=\''.$att_value.'\']'; // create XPath expression for element with certain attribute value
  }

  // Process children, if any.
  foreach($myXMLElement->children() as $childElement) {
//  $xpathList[]= ($currentXPath!=''?$currentXPath.'/':'').(string) $childElement->getName(); // create XPath expression for element
    if ($childValue = (string) $childElement->title) {
      $append = "/text()='$childValue'";
    } else {
      $append = '';
    }

    $xpathList[]= ($currentXPath!=''?$currentXPath.'/':'').(string) $childElement->getName().$append; // create XPath expression for element
    if ($childElement instanceof SimpleXMLElement) {  // if child is an XML node itself then go into recursion
      $xpathList = array_merge($xpathList,XMLElementToXPath(($currentXPath!=''?$currentXPath.'/':'').(string)$childElement->getName(),$childElement));
    }
  }
  return $xpathList;
}
