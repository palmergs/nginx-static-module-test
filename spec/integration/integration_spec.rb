require "spec_helper"

describe "Integration Specs" do
  describe "Bootstrap" do
    it "Webserver is running" do
      expect(Curl.get("http://127.0.0.1:8888").response_code).to eq(200)
    end
  end
end
