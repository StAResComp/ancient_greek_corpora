<?php

// return list of corpora available
function loadCorpora() { //{{{
    global $db;
    
    $sql = 'SELECT corpus_id, corpus_name FROM corpora ORDER BY corpus_name;';
    $rows = $db->query($sql);
    
    $output = '<option value=""/>';
    foreach ($rows as $row) {
        $output .= sprintf('<option value="%d">%s</option>',
                           $row['corpus_id'], $row['corpus_name']);
    }
    
    return $output;
}
//}}}

// return list of collocations available for corpus
function loadCollocDefs($corpus) { //{{{
    global $db;
    
    $sql = 'SELECT collocation_def_id, min_occurrences, min_length FROM collocation_definitions WHERE corpus_id = ?;';
    $stmt = $db->prepare($sql);
    $stmt->execute(array($corpus));
    $rows = $stmt->fetchAll();
    
    $output = '<option value=""/>';
    foreach ($rows as $row) {
        $output .= sprintf('<option value="%d">min occurrences: %d, min length: %d</option>',
                           $row['collocation_def_id'],
                           $row['min_occurrences'], $row['min_length']);
    }
    
    return $output;
}
//}}}

// return list of words matching start of given word
function suggestWords($word, $collocDefID, $word1ID, $callback) { //{{{
    global $db;
    
    $sql = '';
    $params = array();
    
    // need first word
    if ('' == $word1ID) {
        $sql = 'SELECT DISTINCT unique_id AS id, form AS label, form AS value FROM collocation_words INNER JOIN unique_words USING (unique_id) INNER JOIN collocations USING (collocation_id) WHERE collocation_def_id = ? AND form LIKE ? ORDER BY form ASC;';
        $params = array($collocDefID, sprintf('%s%%', $word));
    }
    // have first word, so get second
    else {
        $sql = 'SELECT DISTINCT unique_id AS id, form AS label, form AS value FROM collocation_words INNER JOIN unique_words USING (unique_id) INNER JOIN collocations USING (collocation_id) WHERE collocation_def_id = ? AND form like ? AND collocation_id IN (SELECT collocation_id FROM collocation_words INNER JOIN collocations USING (collocation_id) WHERE unique_id = ? AND collocation_def_id = ?) ORDER BY form ASC';
        $params = array($collocDefID, sprintf('%s%%', $word), $word1ID, $collocDefID);
    }
    
    $stmt = $db->prepare($sql);
    $stmt->execute($params);
    $rows = $stmt->fetchAll(PDO::FETCH_ASSOC);
    
    return sprintf('%s(%s)', $callback, json_encode($rows));
}
//}}}

// return lines containing the chosen words
function getResults($wordIDs, $collocDefID) { //{{{
    global $db;
    
    $sql = 'SELECT DISTINCT text_name, line_number, form, words.unique_id, GROUP_CONCAT(DISTINCT collocation_words.unique_id) AS unique_ids FROM texts INNER JOIN lines USING (text_id) INNER JOIN words USING (line_id) INNER JOIN unique_words USING (unique_id) INNER JOIN collocation_lines USING (line_id) INNER JOIN collocations USING (collocation_id) INNER JOIN collocation_words USING (collocation_id) WHERE collocation_def_id = ? AND collocation_id IN (SELECT DISTINCT c.collocation_id FROM collocations AS c %s WHERE %s) GROUP BY collocation_id, text_name, line_number, form ORDER BY collocation_id, text_id, line_number, word_number;';
    
    // fill in place holders
    $innerJoinFmt = ' INNER JOIN collocation_words AS w%d ON c.collocation_id = w%d.collocation_id ';
    $whereFmt = ' w%d.unique_id = ? ';
    
    $innerJoin = '';
    $where = '';
    
    // build up SQL clauses based on number of word IDs supplied
    for ($i = 0; $i < count($wordIDs); ++ $i) {
        if (0 != $i) {
            $where .= ' AND ';
        }
        
        $innerJoin .= sprintf($innerJoinFmt, $i, $i);
        $where .= sprintf($whereFmt, $i);
    }
    
    // put SQL together
    $sql = sprintf($sql, $innerJoin, $where);
    
    // params for executing SQL statement
    $params = array_merge(array($collocDefID), $wordIDs);
    
    $stmt = $db->prepare($sql);
    $stmt->execute($params);
    $rows = $stmt->fetchAll();
    
    $output = '';
    $old = '';
    $oldColloc = '';
    $lines = 0;
    
    // loop over lines returned
    foreach ($rows as $row) {
        // concat identifiers for this line
        $hash = $row['text_name'] . $row['line_number'] . $row['unique_ids'];
        
        if ('' != $oldColloc && $oldColloc != $row['unique_ids']) {
            $output .= '<hr/>';
        }
        
        // new text or line, so start new <p>
        if ($old != $hash) {
            $output .= sprintf('</p><p><em>%s</em> (%d) - ', 
                               $row['text_name'], $row['line_number']);
            
            // remember text/line/collocation
            $old = $hash;
            $oldColloc = $row['unique_ids'];
            ++ $lines;
        }
        
        $ids = explode(',', $row['unique_ids']);
        
        // add current word - put words used in collocation in bold
        $fmt = in_array($row['unique_id'], $ids) ? '<strong>%s</strong> ' : '%s ';
        $output .= sprintf($fmt, $row['form']);
    }

    return sprintf('<p>There were %d results.%s</p>', $lines, $output);
}
//}}}

$output = '';
$dbFile = '/home/cs2/greek/repo/build/words.db';
$dsn = sprintf('sqlite:%s', $dbFile);

do {
    // open database
    $db = new PDO($dsn);
    if (!$db) {
        $output = 'couldn\'t open DB';
        break;
    }
    
    // decide what to do
    switch (isset($_REQUEST['action']) ? $_REQUEST['action'] : '') {
     case 'loadCorpora':
        $output = loadCorpora();
        break;
        
     case 'loadCollocDefs':
        if (isset($_REQUEST['corpus'])) {
            $output = loadCollocDefs($_REQUEST['corpus']);
        }
        break;
        
     case 'suggestWords':
        if (isset($_REQUEST['word']) && isset($_REQUEST['collocDef']) &&
            isset($_REQUEST['word1ID']) && isset($_REQUEST['callback'])) {
            // sending back JSONP
            header('Content-Type: text/javascript; charset=utf8');
            header('Access-Control-Allow-Origin: *');
            
            $output = suggestWords($_REQUEST['word'], $_REQUEST['collocDef'],
                                   $_REQUEST['word1ID'], $_REQUEST['callback']);
        }
        break;
        
     case 'getResults':
        if (isset($_REQUEST['wordIDs']) && isset($_REQUEST['collocDef'])) {
            $output = getResults(explode(' ', trim($_REQUEST['wordIDs'])), 
                                 $_REQUEST['collocDef']);
        }
        break;
        
     default:
        break;
    }
} while (false);

print $output;

?>