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
    
    // SQL for getting words matching $word% in the given collocation definition
    $sql = <<<SQL
      SELECT DISTINCT unique_id AS id, form AS label, form AS value 
      FROM collocation_words 
      INNER JOIN unique_words USING (unique_id) 
      INNER JOIN collocations USING (collocation_id) 
      WHERE collocation_def_id = ? AND form LIKE ? %s 
      ORDER BY form ASC;
SQL;
    
    $inClause = '';
    $params = array($collocDefID, sprintf('%s%%', $word));
    
    // have first word, so get second
    if ('' != $word1ID) {
        // use IN clause to limit second words to those in collocations containing first word
        $inClause = <<<SQL
          AND collocation_id IN (SELECT collocation_id 
                                 FROM collocation_words 
                                 INNER JOIN collocations USING (collocation_id) 
                                 WHERE unique_id = ? AND collocation_def_id = ?)
SQL;
        
        // add more params
        $params = array_merge($params, array($word1ID, $collocDefID));
    }
    
    // merge in IN clause (empty if not needed)
    $sql = sprintf($sql, $inClause);
    
    $stmt = $db->prepare($sql);
    $stmt->execute($params);
    $rows = $stmt->fetchAll(PDO::FETCH_ASSOC);
    
    // send back words as JSON wrapped in callback function
    return sprintf('%s(%s)', $callback, json_encode($rows));
}
//}}}

// return lines containing the chosen words
function getResults($wordIDs, $collocDefID) { //{{{
    global $db;

    $output = '';
    $lines = 0;

    do {
        if (!$wordIDs) {
            break;
        }
        
        $sql = <<<SQL
          SELECT DISTINCT text_name, line_number, form, words.unique_id, GROUP_CONCAT(DISTINCT collocation_words.unique_id) AS unique_ids 
          FROM texts 
          INNER JOIN lines USING (text_id) 
          INNER JOIN words USING (line_id) 
          INNER JOIN unique_words USING (unique_id) 
          INNER JOIN collocation_lines USING (line_id) 
          INNER JOIN collocations USING (collocation_id) 
          INNER JOIN collocation_words USING (collocation_id) 
          WHERE collocation_def_id = ? AND collocation_id IN (SELECT DISTINCT c.collocation_id 
                                                              FROM collocations AS c %s 
                                                              WHERE %s) 
          GROUP BY collocation_id, text_name, line_number, form 
          ORDER BY collocation_id, text_id, line_number, word_number;
SQL;
        
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
        
        $old = '';
        $oldColloc = '';
        
        // loop over lines returned
        foreach ($rows as $row) {
            // concat identifiers for this line
            $hash = $row['text_name'] . $row['line_number'] . $row['unique_ids'];
            
            // put <hr/> between collocations
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
    } while (false);
        
    return sprintf('<p>There were %d results.%s</p>', $lines, $output);
}
//}}}

$output = '';

do {
    $dbFile = '/home/cs2/greek/repo/build/words.db';
    $dsn = sprintf('sqlite:%s', $dbFile);

    // fields needed by each action
    $fields = array('loadCorpora' => array(),
                    'loadCollocDefs' => array('corpus'),
                    'suggestWords' => array('word', 'collocDef', 'word1ID', 
                                            'callback'),
                    'getResults' => array('wordIDs', 'collocDef'));
    
    // open database
    $db = new PDO($dsn);
    if (!$db) {
        $output = 'couldn\'t open DB';
        break;
    }
    
    $action = isset($_REQUEST['action']) ? $_REQUEST['action'] : '';
    if (!isset($fields[$action])) {
        $output = 'unknown action';
        break;
    }
    
    // check that all fields needed by action are present
    foreach ($fields[$action] as $f) {
        if (!isset($_REQUEST[$f])) {
            $output = sprintf('field %s for action %s is missing', 
                              $f, $action);
            break 2;
        }
    }
    
    // decide what to do
    switch ($action) {
     case 'loadCorpora':
        $output = loadCorpora();
        break;
        
     case 'loadCollocDefs':
        $output = loadCollocDefs($_REQUEST['corpus']);
        break;
        
     case 'suggestWords':
        // sending back JSONP
        header('Content-Type: text/javascript; charset=utf8');
        header('Access-Control-Allow-Origin: *');
        
        $output = suggestWords($_REQUEST['word'], $_REQUEST['collocDef'],
                               $_REQUEST['word1ID'], $_REQUEST['callback']);
        break;
        
     case 'getResults':
        $output = getResults(explode(' ', trim($_REQUEST['wordIDs'])), 
                             $_REQUEST['collocDef']);
        break;
        
     default:
        break;
    }
} while (false);

print $output;

?>