/**
 * 从模板创建新的 story（只修改 summary 和 description）
 * @param templateStoryId 模板 story 的 ID
 * @param newSummary 新的 summary
 * @param newDescription 新的 description（可选，传 null 则使用模板的）
 * @return 新创建的 story key
 */
public String createStoryFromTemplate(String templateStoryId, String newSummary, String newDescription) {
    try {
        // 1. 获取模板 story 的完整信息
        String getUrl = String.format("%s/rest/api/2/issue/%s", jiraBaseUrl, templateStoryId);
        HttpResponse response = httpClient.httpRequest(getUrl, method: "GET", jsonPayload: null);
        
        JsonObject issue = JsonParser.parseString(response.getText()).getAsJsonObject();
        JsonObject fields = issue.getAsJsonObject("fields");
        
        // 2. 构建新 story 的字段（完全复制模板）
        Map<String, Object> newFields = new HashMap<>();
        Gson gson = new Gson();
        
        // 复制所有字段
        for (Map.Entry<String, JsonElement> entry : fields.entrySet()) {
            String fieldKey = entry.getKey();
            JsonElement fieldValue = entry.getValue();
            
            // 跳过只读字段
            if (isReadOnlyField(fieldKey) || fieldValue.isJsonNull()) {
                continue;
            }
            
            // 转换为标准 Java 对象
            Object value = gson.fromJson(fieldValue, Object.class);
            newFields.put(fieldKey, value);
        }
        
        // 3. 必需字段特殊处理（确保格式正确）
        Map<String, Object> project = new HashMap<>();
        project.put("key", fields.getAsJsonObject("project").get("key").getAsString());
        newFields.put("project", project);
        
        Map<String, Object> issuetype = new HashMap<>();
        issuetype.put("name", fields.getAsJsonObject("issuetype").get("name").getAsString());
        newFields.put("issuetype", issuetype);
        
        // 4. 只修改 summary 和 description
        newFields.put("summary", newSummary);
        if (newDescription != null) {
            newFields.put("description", newDescription);
        }
        
        // 5. 创建新 story
        Map<String, Object> payload = new HashMap<>();
        payload.put("fields", newFields);
        
        String createUrl = String.format("%s/rest/api/2/issue/", jiraBaseUrl);
        String requestBody = gson.toJson(payload);
        
        HttpResponse createResponse = httpClient.httpRequest(createUrl, method: "POST", jsonPayload: requestBody);
        JsonObject createdIssue = JsonParser.parseString(createResponse.getText()).getAsJsonObject();
        
        return createdIssue.get("key").getAsString();
        
    } catch (Exception e) {
        throw new RuntimeException("Failed to create story from template: " + e.getMessage(), e);
    }
}

/**
 * 判断是否为只读字段（不能在创建时设置）
 */
private boolean isReadOnlyField(String fieldKey) {
    List<String> readOnlyFields = Arrays.asList(
        "created", "updated", "creator", "reporter", 
        "resolutiondate", "lastViewed", "watches",
        "votes", "worklog", "comment", "issuelinks",
        "attachment", "subtasks", "aggregatetimeestimate",
        "aggregatetimespent", "aggregateprogress", "progress",
        "timetracking", "workratio", "status", "statuscategorychangedate"
    );
    return readOnlyFields.contains(fieldKey);
}
