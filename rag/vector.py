import os

import chromadb
import ollama
from chromadb import Settings
from langchain_ollama import OllamaEmbeddings
from langchain_chroma import Chroma
from langchain_core.documents import Document
from langchain_community.document_loaders import DirectoryLoader
import pandas as pd
from langchain_text_splitters import CharacterTextSplitter



documents = [
  "Llamas are members of the camelid family meaning they're pretty closely related to vicuñas and camels",
  "Llamas were first domesticated and used as pack animals 4,000 to 5,000 years ago in the Peruvian highlands",
  "Llamas can grow as much as 6 feet tall though the average llama between 5 feet 6 inches and 5 feet 9 inches tall",
  "Llamas weigh between 280 and 450 pounds and can carry 25 to 30 percent of their body weight",
  "Llamas are vegetarians and have very efficient digestive systems",
  "Llamas live to be about 20 years old, though some only live for 15 years and others live to be 30 years old",
]

class ChromadbClient:
    def __init__(self, persist_directory: str):
        self.client = chromadb.Client(Settings(persist_directory=persist_directory))
        self.collection = self.client.create_collection(name="docs")

    def load_documents(self):
        for i, d in enumerate(documents):
            response = ollama.embed(model="mxbai-embed-large", input=d)
            embeddings = response["embeddings"]
            self.collection.add(
                ids=[str(i)],
                embeddings=embeddings,
                documents=[d]
            )

    def query(self, query_text: str, n_results: int = 5):
        response = ollama.embed(model="mxbai-embed-large", input=query_text)
        query_embedding = response["embeddings"][0]
        results = self.collection.query(
            query_embeddings=query_embedding,
            n_results=n_results
        )
        return results



# store each document in a vector embedding database


# embeddings = OllamaEmbeddings(model="mxbai-embed-large")
# db_location = "./chroma_db_rabbitmq_db"
#
# client = chromadb.Client(Settings(persist_directory="./data/"))
#
# collection = client.create_collection(name="sources")
# student_info = """
# Alexandra Thompson, a 19-year-old computer science sophomore with a 3.7 GPA,
# is a member of the programming and chess clubs who enjoys pizza, swimming, and hiking
# in her free time in hopes of working at a tech company after graduating from the University of Washington.
# """
#
# club_info = """
# The university chess club provides an outlet for students to come together and enjoy playing
# the classic strategy game of chess. Members of all skill levels are welcome, from beginners learning
# the rules to experienced tournament players. The club typically meets a few times per week to play casual games,
# participate in tournaments, analyze famous chess matches, and improve members' skills.
# """
#
# university_info = """
# The University of Washington, founded in 1861 in Seattle, is a public research university
# with over 45,000 students across three campuses in Seattle, Tacoma, and Bothell.
# As the flagship institution of the six public universities in Washington state,
# UW encompasses over 500 buildings and 20 million square feet of space,
# including one of the largest library systems in the world.
# """
#
# collection.add(
#     documents = [student_info, club_info, university_info],
#     metadatas = [{"source": "student info"},{"source": "club info"},{'source':'university info'}],
#     ids = ["id1", "id2", "id3"],
# )


# loader = DirectoryLoader('./data/', glob="**/*.txt")
# documents = loader.load()
# text_splitter = CharacterTextSplitter(chunk_size=1000, chunk_overlap=0)
# texts = text_splitter.split_documents(documents)


# vectorstore = Chroma.from_documents(documents=texts,
#                                     embedding=embeddings,
#                                     persist_directory="./chroma_db")
#
# # retriver = vectorstore.as_retriever(search_type="similarity", search_kwargs={"k": 5})

# add_documents = not os.path.exists(db_location)
#
# if add_documents:
#     documents = []
#     ids = []
#     for index, row in df.iterrows():
#         doc = Document(
#             page_content=row["Title"] + " " + row["Review"],
#             metadata={"rating": row["Rating"], "date": row["Date"]},
#             id = str(index)
#         )
#         ids.append(str(index))
#         documents.append(doc)
#
# vectorstore = Chroma.from_documents(
#     collection_name="rabbitmq",
#     embedding=embeddings,
#     persist_directory=db_location,
#     # documents=documents,
# )
#
# if add_documents:
#     vectorstore.add_documents(documents=documents, ids=ids)
#
# retriver = vectorstore.as_retriever(search_type="similarity", search_kwargs={"k":5})