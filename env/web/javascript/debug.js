$(document).ready(function(){
	$.ajax("/ajax/db/tables", {
		dataType:"json",
		success:function(j){
			var html = "";
			if (j) {
				for (var v in j) {
					html += "<li>"+j[v].name+"</li>";
				}
			}
			$("#dbTableList").html(html);
		}
	});
	if (typeof $.QSA.table !== "undefined") {
		$.ajax("/ajax/db/tables?table="+$.QSA.table, {
			dataType:"json",
			success:function(j){
				var html,th="",tb="";
				if (j) {
					if (j["columns"]) {
						for (var v in j["columns"]) {
							th += "<th>"+j["columns"][v].name+"</th>";
						}
					}
					/*for (var v in j) {
						html += "<li>"+j[v].name+"</li>";
					}*/
					if (th || tb) {
						html = "<table><thead><tr>"+th+"</tr></thead><tbody>"+tb+"</tbody></table>";
					}
				}
				$("#table").html(html);
			}
		});
	}
});