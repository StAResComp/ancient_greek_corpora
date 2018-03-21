// names of fields
const corpora = '#corpora';
const collocDefs = '#collocation_definitions';
const word1 = '#word1';
const word2 = '#word2';
const results = '#results';
const getResultsID = '#getResults';
const ajaxURL = 'ajax.php';

// run on load
window.onload = function (event) {
     loadCorpora();
     autocomplete(word1);
     autocomplete(word2);
     getResultsTrigger();
}

// add click event to button
function getResultsTrigger() { //{{{
     $(getResultsID).on('click', getResults);
}
//}}}

// get results using IDs of chosen words
function getResults() { //{{{
     const formData = {
          'action': 'getResults',
          'wordIDs': $(word1 + 'id').val() + ' ' + $(word2 + 'id').val(),
          'collocDef': $(collocDefs).val()
     };
     
     $.ajax({
          'type': 'GET',
          'url': ajaxURL,
          'dataType': 'html',
          'data': formData,
          
          'success': function(data) {
               $(results).html(data);
          }
     });
}
//}}}

// set up autocomplete
function autocomplete(selector) { //{{{
     $(selector).autocomplete({
          'source': function(request, response) {
               var formData = {
                    'action': 'suggestWords',
                    'word': $(selector).val(),
                    'collocDef': $(collocDefs).val(),
                    'word1ID': $(word1 + 'id').val()
               };
               
               $.ajax({
                    'type': 'GET',
                    'url': ajaxURL,
                    'dataType': 'jsonp',
                    'data': formData,
                    
                    'success': function(data) {
                         response(data);
                    }
               });
               
               $(selector + 'id').val('');
          },
          'minLength': 1,
          'select': function(event, ui) {
               $(selector + 'id').val(ui.item.id);
               
               // on second word, so can get results
               if (word2 == selector) {
                    getResults();
               }
          }
     });
}
//}}}

// load collocation definitions using corpus
function loadCollocationDefs() { //{{{
     const formData = {
          'action': 'loadCollocDefs',
          'corpus': $(corpora).val()
     };
     
     $.ajax({
          'type': 'GET',
          'url': ajaxURL,
          'dataType': 'html',
          'data': formData,
          
          'success': function(data) {
               $(collocDefs).html(data);
          }
     });
}
//}}}

// load available corpora
function loadCorpora() { //{{{
     const formData = {
          'action': 'loadCorpora'
     };
     
     $.ajax({
          'type': 'GET',
          'url': ajaxURL,
          'dataType': 'html',
          'data': formData,
          
          'success': function(data) {
               $(corpora).html(data);
          }
     });
     
     $(corpora).on('change', loadCollocationDefs);
}
//}}}