<?php

$collocDefs = "collocation_definitions";
$corpora = 'corpora';
$word1 = 'word1';
$word2 = 'word2';

$form = <<<EOT
  <form method="get" action="index.php">
    <p>
      <label for="${corpora}">Corpora available</label>
      <select name="${corpora}" id="{$corpora}">
      </select>
    </p>
    <p>
      <label for="${collocDefs}">Collocation definitions</label>
      <select name="${collocDefs}" id="{$collocDefs}">
      </select>
    </p>
    <p>
      <label for="${word1}">First word</label>
      <input type="text" name="${word1}" id="${word1}"/>
      <input type="hidden" name="${word1}id" id="${word1}id"/>
      <input type="button" name="getResults" id="getResults" value="Get collocations"/>
    </p>
    <p>
      <label for="${word2}">Second word</label>
      <input type="text" name="${word2}" id="${word2}"/>
      <input type="hidden" name="${word2}id" id="${word2}id"/>
    </p>
  </form>
EOT;

$results = "";

?>

<html>
  <head>
    <title>Collocations</title>
    <script src="https://code.jquery.com/jquery-3.3.1.min.js"></script>
    <script src="https://code.jquery.com/ui/1.12.1/jquery-ui.min.js"></script>
    <script src="ajax.js"></script>
    <style type="text/css">
      .ui-helper-hidden-accessible { display: none; }
      .ui-autocomplete { background: #fff; border: 1px solid #000; width: 10em; }
    </style>
  </head>
  <body>
    <h1>Collocations</h1>
    <p>First choose a corpus, and then a collocation definition (the minimum number of occurrences of the collocation and the minimum length of the collocation).</p>
    <p>Then enter either one or two words which the collocation must contain. As you start entering a word, suggestions appear. Select one of these.</p>
    <p>If just using one word, hit the <code>Get collocations</code> button, otherwise selecting the second word will trigger getting the collocations automatically.</p>
    <?php echo $form; ?>
    <div id="results"></div>
  </body>
</html>