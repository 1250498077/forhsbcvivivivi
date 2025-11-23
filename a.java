


public List<String> getCreatableCustomFields(String projectKey, String issueType) {
    // 获取创建 issue 时可用的字段
    String metaUrl = "https://wpb-jira.systems.uk.hsbc/rest/api/2/issue/createmeta?projectKeys=${projectKey}&issuetypeNames=${issueType}&expand=projects.issuetypes.fields";
    
    HttpResponse httpResponse = httpClient.httpRequest(metaUrl, method: "GET");
    def meta = new JsonSlurper().parseText(httpResponse.getText());
    
    def fields = meta.projects[0]?.issuetypes[0]?.fields;
    
    // 返回所有可创建的字段（包括自定义字段）
    return fields.keySet().findAll { it.startsWith("customfield_") };
}
public String createStoryFromTemplate(String templateStoryId, String newSummary, String newDescription) {
    try {
        // 1. 获取模板 story（使用 issue API）
        String getUrl = String.format("%s/rest/api/2/issue/%s", jiraBaseUrl, templateStoryId);
        HttpResponse response = httpClient.httpRequest(getUrl, method: "GET", jsonPayload: null);
        
        JsonObject issue = JsonParser.parseString(response.getText()).getAsJsonObject();
        JsonObject fields = issue.getAsJsonObject("fields");
        
        // 2. 构建新 story 的字段
        Map<String, Object> newFields = new HashMap<>();
        Gson gson = new Gson();
        
        // 必需字段
        Map<String, String> project = new HashMap<>();
        project.put("key", fields.getAsJsonObject("project").get("key").getAsString());
        newFields.put("project", project);
        
        Map<String, String> issuetype = new HashMap<>();
        issuetype.put("name", fields.getAsJsonObject("issuetype").get("name").getAsString());
        newFields.put("issuetype", issuetype);
        
        // 修改的字段
        newFields.put("summary", newSummary);
        if (newDescription != null) {
            newFields.put("description", newDescription);
        } else if (fields.has("description") && !fields.get("description").isJsonNull()) {
            newFields.put("description", fields.get("description").getAsString());
        }
        
        // 复制其他字段
        List<String> fieldsToCopy = Arrays.asList("priority", "assignee", "labels", "components");
        for (String fieldKey : fieldsToCopy) {
            if (fields.has(fieldKey) && !fields.get(fieldKey).isJsonNull()) {
                Object value = gson.fromJson(fields.get(fieldKey), Object.class);
                newFields.put(fieldKey, value);
            }
        }
        
        // 复制自定义字段（排除只读字段）
        for (Map.Entry<String, JsonElement> entry : fields.entrySet()) {
            String fieldKey = entry.getKey();
            if (fieldKey.startsWith("customfield_") && !entry.getValue().isJsonNull() && !isReadOnlyField(fieldKey)) {
                Object value = gson.fromJson(entry.getValue(), Object.class);
                newFields.put(fieldKey, value);
            }
        }
        
        // 3. 创建新 story
        Map<String, Object> payload = new HashMap<>();
        payload.put("fields", newFields);
        
        String createUrl = String.format("%s/rest/api/2/issue/", jiraBaseUrl);
        String requestBody = gson.toJson(payload);
        
        HttpResponse createResponse = httpClient.httpRequest(createUrl, method: "POST", jsonPayload: requestBody);
        JsonObject createdIssue = JsonParser.parseString(createResponse.getText()).getAsJsonObject();
        
        return createdIssue.get("key").getAsString();
        
    } catch (Exception e) {
        throw new RuntimeException("Failed to create story: " + e.getMessage(), e);
    }
}



private boolean isReadOnlyField(String fieldKey) {
    // 某些自定义字段是系统生成的，不能在创建时设置
    List<String> readOnlyFields = [
        "customfield_12077",  // 例如：自动计算的字段
        "archiveddate"
    ];
    return readOnlyFields.contains(fieldKey);
}
